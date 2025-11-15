#include "onlineGame.hpp"
#include "../global.hpp"
#include "../ServerSession.hpp"
#include "../SendBuffer.hpp"

extern HWND ghWnd;
extern RECT gClientRect;

namespace Online {

Game::Game() {
	// 스레드 풀 초기화
	std::cout << "----------[게임 초기화 설정]----------\n";
	std::cout << "스레드 풀에 사용할 스레드 수를 입력해 주세요.\n";
	std::cout << "컴퓨터의 물리 코어 수: " << numberOfPhysicalCores() << '\n';
	std::cout << "사용 가능한 물리 코어 수: " << numberOfPhysicalCores() - 1 << " (1개 - 메인 스레드)\n";
	std::cout << "스레드 수: ";

	std::size_t threadCnt{};
	std::cin >> threadCnt;

	threadPool_.run(threadCnt);

	// GFX 객체 초기화
	gGfx.setupDXGI(D3D_FEATURE_LEVEL_12_1);
	gGfx.init();
	gGfx.createSwapChain();
	gGfx.setThreadPool(&threadPool_);

	gGfx.loadMeshes();
}

void Game::setupStage() {
	cubes_.resize(8u);
	for ( auto& plane : cubes_ ) {
		plane.resize( 9u );
		for ( auto& row : plane ) {
			row.resize( 9u );
		}
	}

	for ( std::size_t i = 0u; i < cubes_.size( ); ++i ) {
		for ( std::size_t j = 0u; j < cubes_[ i ].size( ); ++j ) {
			for ( std::size_t k = 0u; k < cubes_[ i ][ j ].size( ); ++k ) {
				cubes_[ i ][ j ][ k ].setMesh( gGfx.cubeMesh( ) );
				cubes_[ i ][ j ][ k ].setPos( mu::Vec3(
					( static_cast<int>( k ) - static_cast<int>( cubes_.size( ) / 2 ) ) * 0.5f,
					( static_cast<int>( j ) - static_cast<int>( cubes_.size( ) / 2 ) ) * 0.5f,
					( static_cast<int>( i ) - static_cast<int>( cubes_.size( ) / 2 ) ) * 0.5f
				) );
				cubes_[ i ][ j ][ k ].setOmega( mu::Vec3( rand( -1.f, 1.f ), rand( -1.f, 1.f ), rand( -1.f, 1.f ) ) );
				cubes_[ i ][ j ][ k ].setScale( 0.05f );
			}

		}
	}

	//gPlayer = std::make_shared<Object>();
	gPlayer->setPos(mu::Vec3(0.f, 0.f, 0.f));
	gPlayer->setModel(gGfx.modelPlayer());
	gPlayer->setScale(0.15f);

	dirLight_.setOrient(mu::NQuat(mu::Degree(0.f), mu::Degree(60.f), mu::Degree(15.f)));
	dirLight_.color = mu::Vec3(0.8f, 0.8f, 0.8f);
	dirLight_.intensity = 1.f;
	dirLight_.type = PBRPipeline::LightData::Type::DirectionalLight;

	camera_.setTargetObject( gPlayer );
	camera_.setOffsetFromTarget( mu::Vec3( 0.f, 0.2f, -0.5f ) );
	camera_.setPerspective( mu::Degree( 90.f ),
		static_cast<float>( gClientRect.right - gClientRect.left ) / ( gClientRect.bottom - gClientRect.top ),
		0.025f, 8.f
	);
}

void Game::update(Milliseconds deltaTime) {
	processInput(deltaTime);

	for ( auto& plane : cubes_ ) {
		for ( auto& row : plane ) {
			for ( auto& cube : row ) {
				cube.update( deltaTime );
			}
		}
	}

	gPlayer->update(deltaTime);
	for( auto& [id, object] : gObjects ) {
		if( object != gPlayer ) {
			object->update( deltaTime );
		}
	}

	camera_.update();
	dirLight_.update(deltaTime);
}

void Game::render() {
	for ( auto& plane : cubes_ ) {
		for ( auto& row : plane ) {
			for ( auto& cube : row ) {
				cube.render( gGfx );
			}
		}
	}

	gPlayer->render(gGfx);
	for( auto& [id, object] : gObjects ) {
		if( object != gPlayer ) {
			object->render( gGfx );
		}
	}

	camera_.updateGFX(gGfx);
	dirLight_.render(gGfx);

	auto frameData = PBRPipeline::FrameData{
		.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
	};
	gGfx.addFrameData( frameData );

	gGfx.render();
}

void Game::processInput(Milliseconds deltaTime) {
	auto packet = Packet{
		.header = {
			.size = sizeof( PacketHeader ) + sizeof( CSMovePacket ),
			.id = static_cast<std::uint16_t>( PacketType::csMove )
		},
		.csMove = {
			.dir = Direction::none
		}
	};

	if ( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState('W') & 0x8000 ) {
		packet.csMove.dir = Direction::w;
	}
	if ( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState('A') & 0x8000 ) {
		packet.csMove.dir = Direction::a;
	}
	if ( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState('S') & 0x8000 ) {
		packet.csMove.dir = Direction::s;
	}
	if ( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState('D') & 0x8000 ) {
		packet.csMove.dir = Direction::d;
	}

	if ( packet.csMove.dir == Direction::none ) {
		return;
	}

	i32t packetSize = sizeof( Packet );
	auto buffer = std::make_shared<SendBuffer>( packetSize );
	buffer->copyData( &packet, packetSize );
	gServerSession->send( buffer );
}

}	// namespace Online