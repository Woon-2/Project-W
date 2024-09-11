#include "game.hpp"

#include "mouseWin32Adaptor.hpp"
#include "testInput.hpp"

#include "d3d12scene.hpp"

#include "mygfx.hpp"

#include "d3d12inputLayoutPresets.hpp"

#include "resourcePath.hpp"

#include <chrono>
#include <thread>

#include "phongShader.hpp"

using namespace std::literals;

enum class PACKET_TYPE : std::uint8_t {
    HELLO,
    UPDATE,
    LEAVE
};

#pragma pack(push, 1)

struct HelloClientPacket {
    PACKET_TYPE type;
};

struct HelloServerPacket {
    PACKET_TYPE type;
    std::uint32_t id;
    World world;
};

struct UpdateClientPacket {
    PACKET_TYPE type;
    World::Obj obj;
};

struct UpdateServerPacket {
    PACKET_TYPE type;
    World world;
};

struct LeavePacket {
    PACKET_TYPE type;
};

#pragma pack(pop)

Game::Game(gfx::ICore& gfx, MyWindow& wnd, ic::Mouse& mouse, ic::Keyboard& keyboard)
    : world_{}, timer_(), baseCoordSys_(), camera_(gfx::Camera::Config{
        .fov = 90.f, .aspect = wnd.client().width / static_cast<float>(wnd.client().height),
        .near = 0.1f, .far = 1000.f
    }), guns_(), /*inputSystem_(keyboard),*/ physicsSystem_(keyboard), socket_(), pGfx_(&gfx), pWnd_(&wnd),
    pMouse_(&mouse), renderFunc_(&Game::initialRender), lockFPS_(defLockFPS), player_(),
    curFenceIdx_(0), prevFenceIdx_(1u), networkID_(-1) {
    socket_.connect(net::SockAddr(net::Ipv4Addr(), net::Port(55555u)));
    net::enNonb(socket_);

    std::array<char, 1> buffer;
    buffer[0] = static_cast<char>(PACKET_TYPE::HELLO);
    socket_.sendUc(buffer.data(), buffer.size());

    setupWndMsgHandlers();
    loadAssets();
    setupCamera();
    initECS();
    initLights();

    player_.Init();
}

Game::~Game() {
	std::array<char, 1> buffer;
	buffer[0] = static_cast<char>(PACKET_TYPE::LEAVE);
	net::disNonb(socket_);
	socket_.send(buffer.data(), buffer.size());
}

void Game::update() {
    processNetwork();
    processInput();
    timer_.update();

    const auto expectedFrameTime = 1. / lockFPS_;
    const auto restFrameTime = expectedFrameTime - timer_.GetDT();

    if (restFrameTime > 0.) {
        std::this_thread::sleep_for( std::chrono::duration<double>(restFrameTime) );
    }

    pWnd_->setTitle(timer_.str());

    camera_.updateView();

    auto& playerRigidBody =  ecs::GetComponent<Rigidbody>(player_.entityNumber_);
    const auto playerOldPos = ecs::GetComponent<Position>(player_.entityNumber_);
    const auto playerDeltaPos = playerRigidBody.deltaPosition();
    guns_[networkID_].coord() << mu::translate(playerDeltaPos);

    for (std::size_t i = 0; i < guns_.size(); ++i) {
        if (i != networkID_) [[likely]] {
            const auto& obj = world_.obj[i];
            guns_[i].update( mu::Vec3( obj.x, obj.y, obj.z ), mu::NQuat() );
        }
    }

    auto& playerPos = ecs::GetComponent<Position>(player_.entityNumber_);
	playerPos.x = playerOldPos.x + playerDeltaPos.x();
	playerPos.y = playerOldPos.y + playerDeltaPos.y();
	playerPos.z = playerOldPos.z + playerDeltaPos.z();

    baseCoordSys_.traverse();
    
    player_.printPos();
}

void Game::render() {
    (this->*renderFunc_)();
}

void Game::initialRender() {
    const auto expectedFrameTime = 1. / lockFPS_;

    // skip rendering if the frame time is too long
    if (timer_.GetDT() > expectedFrameTime * 2.) {
        return;
    }

    static_cast<gfx::d3d12::PhongShader&>( 
        static_cast<MyGfx&>(*pGfx_).shader( gfx::d3d12::PhongShader::shaderName() )
    ).setFrame(curFenceIdx_);

    auto pRenderContext = pGfx_->createContext();
    pGfx_->preRender();
    pRenderContext->preRender();
    pWnd_->preRender(*pRenderContext);
    pWnd_->clear(*pRenderContext);

    // TODO: camera_->makeScene(world);
    auto scene = gfx::d3d12::CameraScene(camera_);

    auto worlds = std::vector<mu::Mat4x4>{};
    worlds.reserve(guns_.size());

    for (std::size_t i = 0; i < guns_.size(); ++i) {
        if (world_.obj[i].active) {
            const auto& obj = world_.obj[i];
            worlds.push_back( guns_[i].world() );
        }
    }

    scene.addFragment(
        gfx::d3d12::Fragment{
            .pMesh = &Gun::sMesh,
            .worlds = worlds,
            .matIdx = 0
        }
    );

    scene.addMaterial(&Gun::sMaterial);

    for (const auto& light : lights_) {
        scene.addLight(&light);
    }
    
     pGfx_->render( *pRenderContext, scene, static_cast<MyGfx&>(*pGfx_).renderer(
         MyGfx::Renderer::Phong
     ), *pWnd_ );

    pWnd_->postRender(*pRenderContext);
    pRenderContext->postRender();
    pGfx_->postRender();

    auto pd3d12Gfx_ = static_cast<gfx::d3d12::Core*>(pGfx_);
    pd3d12Gfx_->signalGpu(curFenceIdx_);
    std::swap(curFenceIdx_, prevFenceIdx_);

    renderFunc_ = &Game::regularRender;
}

void Game::regularRender() {
    const auto expectedFrameTime = 1. / lockFPS_;

    // skip rendering if the frame time is too long
    if (timer_.GetDT() > expectedFrameTime * 2.) {
        return;
    }

    static_cast<gfx::d3d12::PhongShader&>( 
        static_cast<MyGfx&>(*pGfx_).shader( gfx::d3d12::PhongShader::shaderName() )
    ).setFrame(curFenceIdx_);

    auto pRenderContext = pGfx_->createContext();
    pGfx_->preRender();
    pRenderContext->preRender();
    pWnd_->preRender(*pRenderContext);
    pWnd_->clear(*pRenderContext);

    // TODO: camera_->makeScene(world);
    auto scene = gfx::d3d12::CameraScene(camera_);

    auto worlds = std::vector<mu::Mat4x4>{};
    worlds.reserve(guns_.size());

    for (std::size_t i = 0; i < guns_.size(); ++i) {
        if (world_.obj[i].active) {
            const auto& obj = world_.obj[i];
            worlds.push_back( guns_[i].world() );
        }
    }

    scene.addFragment(
        gfx::d3d12::Fragment{
            .pMesh = &Gun::sMesh,
            .worlds = worlds,
            .matIdx = 0
        }
    );

    scene.addMaterial(&Gun::sMaterial);

    for (const auto& light : lights_) {
        scene.addLight(&light);
    }
    
     pGfx_->render( *pRenderContext, scene, static_cast<MyGfx&>(*pGfx_).renderer(
         MyGfx::Renderer::Phong
     ), *pWnd_ );

    pWnd_->postRender(*pRenderContext);
    pRenderContext->postRender();
    pGfx_->postRender();

    auto pd3d12Gfx_ = static_cast<gfx::d3d12::Core*>(pGfx_);
    pd3d12Gfx_->signalGpu(curFenceIdx_);
    pd3d12Gfx_->waitGpu(prevFenceIdx_);

    pWnd_->present();

    std::swap(curFenceIdx_, prevFenceIdx_);
}

void Game::processInput() {
    while (!pMouse_->empty()) {
        const auto e = pMouse_->read();
        std::ostringstream oss;

        switch(e->type().value()) {
        case ic::Mouse::Event::Type::Move:
            oss << "Mouse Position: (" << e->pos().x << ", " << e->pos().y << ")";
            pWnd_->setTitle(oss.str());
            break;

        case ic::Mouse::Event::Type::LPress:
            pWnd_->setTitle("LPress");
            break;

        case ic::Mouse::Event::Type::LRelease:
            pWnd_->setTitle("LRelease");
            break;

        case ic::Mouse::Event::Type::MPress:
            pWnd_->setTitle("MPress");
            break;

        case ic::Mouse::Event::Type::MRelease:
            pWnd_->setTitle("MRelease");
            break;

        case ic::Mouse::Event::Type::RPress:
            pWnd_->setTitle("RPress");
            break;

        case ic::Mouse::Event::Type::RRelease:
            pWnd_->setTitle("RRelease");
            break;

        case ic::Mouse::Event::Type::WheelUp:
            pWnd_->setTitle("WheelUp");
            break;

        case ic::Mouse::Event::Type::WheelDown:
            pWnd_->setTitle("WheelDown");
            break;

        default:
            break;
        }
    }
    
    // inputSystem_.update();
    physicsSystem_.update(static_cast<float>(timer_.GetDT()));
}

void Game::setupWndMsgHandlers() {
    pWnd_->addMsgHandler(0, std::make_unique<TestInputHandler<MyWindow>>(*pWnd_, *pMouse_, *pGfx_));
    pWnd_->addMsgHandler(1, std::make_unique<ic::Win32::MouseMsgHandler<MyWindow>>(*pWnd_, pMouse_));
}

void Game::loadAssets() {
    auto pd3d12Gfx = static_cast<gfx::d3d12::Core*>(pGfx_);
    Gun::loadAssets(*pd3d12Gfx, curFenceIdx_);
    
    for (std::size_t i = 0; i < 10u; ++i) {
        guns_.emplace_back(&baseCoordSys_);
    }
}

void Game::setupCamera() {
    camera_.coordSys().setParent(&baseCoordSys_);
    camera_.coordSys() << mu::translate(0.f, 1.f, -10.f);
    camera_.focus( gfx::coord::Pt3( &baseCoordSys_, mu::Vec3(0.f, 0.f, 0.f) ) );
}

void Game::initECS() {
    ecs::ConfigEntity();
    ecs::RegisterComponent<Position>();
    ecs::RegisterComponent<PlayerController>();
    ecs::RegisterComponent<Rigidbody>();

    // auto inputSystem_ = ecs::RegisterSystem<InputSystem>();
    auto physicsSystem_ = ecs::RegisterSystem<PhysicsSystem>();

    //{
    //    ecs::Signature signature;
    //    signature.set(ecs::GetComponentType<PlayerController>());
    //    signature.set(ecs::GetComponentType<Position>());
    //    ecs::SetSystemSignature<InputSystem>(signature);
    //}

    {
        ecs::Signature signature;
        signature.set(ecs::GetComponentType<Position>());
        signature.set(ecs::GetComponentType<PlayerController>());
        signature.set(ecs::GetComponentType<Rigidbody>());
        ecs::SetSystemSignature<PhysicsSystem>(signature);
    }

    
}

void Game::processNetwork() {
    static constexpr auto maxPacketSize = 400u;
    std::array<char, maxPacketSize> buffer;
    
    auto bytes = socket_.recvUc(buffer.data(), maxPacketSize);

	if (bytes == 0) {
		return;
	}

    switch (reinterpret_cast<const PACKET_TYPE&>(buffer[0])) {
    case PACKET_TYPE::HELLO: {
        auto pPacket = reinterpret_cast<HelloServerPacket*>(buffer.data());
        networkID_ = pPacket->id;
        std::memcpy(&world_, &pPacket->world, sizeof(world_));
        break;
    }

    case PACKET_TYPE::UPDATE: {
        if (networkID_ == static_cast<std::uint32_t>(-1)) {
            return; // ignore updates until we get our ID
        }

        auto pPacket = reinterpret_cast<UpdateServerPacket*>(buffer.data());
        std::memcpy(&world_, &pPacket->world, sizeof(world_));
        break;
    }

    default:
        break;
    }

    auto& playerPos = ecs::GetComponent<Position>(player_.entityNumber_);
    world_.obj[networkID_].x = static_cast<float>( playerPos.x );
    world_.obj[networkID_].y = static_cast<float>( playerPos.y );
    world_.obj[networkID_].z = static_cast<float>( playerPos.z );

    auto updatePacket = UpdateClientPacket{
        .type = PACKET_TYPE::UPDATE,
        .obj = world_.obj[networkID_]
    };

    std::memset(buffer.data(), 0, buffer.size());
    std::memcpy(buffer.data(), &updatePacket, sizeof(updatePacket));

    socket_.sendUc(buffer.data(), sizeof(updatePacket));
}

void Game::initLights() {
    lights_.push_back( gfx::d3d12::sr::PhongLight{
        .ambient = dx::XMFLOAT4{ 0.51f, 0.54f, 0.57f, 1.f },
        .diffuse = dx::XMFLOAT4{ 0.54f, 0.56f, 0.58f, 1.f },
        .specular = dx::XMFLOAT4{ 0.25f, 0.25f, 0.25f, 1.f },
        .falloff = 1.f,
        .dirV = dx::XMFLOAT3{ -0.1f, -0.6f, 0.4f },
        .type = gfx::d3d12::sr::PhongLight::kTypeDirectional
    } );

    lights_.push_back( gfx::d3d12::sr::PhongLight{
        .ambient = dx::XMFLOAT4{ 0.f, 0.f, 0.f, 1.f },
        .diffuse = dx::XMFLOAT4{ 0.15f, 0.3f, 0.65f, 1.f },
        .specular = dx::XMFLOAT4{ 0.1f, 0.15f, 0.2f, 0.f },
        .posV = dx::XMFLOAT3{ -50.f, 20.f, -5.f },
        .falloff = 8.f,
        .dirV = dx::XMFLOAT3{ 0.f, 0.f, 1.f },
        .cosTheta = std::cos( static_cast<float>( mu::Radian( mu::Degree(25.f) ) ) ),
        .atten = dx::XMFLOAT3{ 1.f, 0.045f, 0.0075f },
        .cosPhi = std::cos( static_cast<float>( mu::Radian( mu::Degree(60.f) ) ) ),
        .type = gfx::d3d12::sr::PhongLight::kTypeSpot
    } );
}