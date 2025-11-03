#include "pch.hpp"
#include "errorHandling.hpp"
#include "gfx.hpp"
#include "object.hpp"
#include "timer.hpp"
#include "camera.hpp"
#include "light.hpp"
#include "ServerSession.hpp"

inline constexpr const char* wndClsName = "wndCls";
inline constexpr const char* wndName = "Project1";

HWND ghWnd = nullptr;
RECT gWndRect{ 0, 0, 1024, 768 };
RECT gClientRect{ 0, 0, 1024, 768 };

LRESULT wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

GFX gGfx{};

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	SocketUtils::init( );
	std::locale::global( std::locale( "ko-KR" ) );

	// 윈도우 클래스 설정 및 윈도우 생성
	auto cls = WNDCLASSEXA{
		.cbSize = sizeof( WNDCLASSEXA ),
		.style = CS_OWNDC,
		.lpfnWndProc = wndProc,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = hInstance,
		.hIcon = nullptr,
		.hCursor = nullptr,
		.hbrBackground = nullptr,
		.lpszMenuName = nullptr,
		.lpszClassName = wndClsName,
		.hIconSm = nullptr
	};

	DISPLAY_ERROR_GLE( RegisterClassExA( &cls ), true );

	AdjustWindowRect( &gWndRect, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, false );

	ghWnd = CreateWindowExA( 0, wndClsName, wndName, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		0, 0, gWndRect.right - gWndRect.left, gWndRect.bottom - gWndRect.top,	// 윈도우의 시작 위치, 크기 설정
		nullptr, nullptr, hInstance, nullptr
	);

	DISPLAY_ERROR_GLE( ghWnd, true );
	DISPLAY_ERROR_GLE( !ShowWindow( ghWnd, SW_SHOW ), true );

	auto threadPool = ThreadPool( );
	threadPool.run( numberOfPhysicalCores( ) - 2u );	// 물리 코어 개수에서 1는 메인 쓰레드, 1는 iocp core용으로 뺌
	std::cout << "ThreadPool runs with " << numberOfPhysicalCores( ) - 2u << " threads (the physical core count - 1)\n";

	// 그래픽스 초기화 - DXGI, D3D12
	//GFX gfx{};
	gGfx.setupDXGI( D3D_FEATURE_LEVEL_12_1 );
	gGfx.init( );
	gGfx.createSwapChain( );

	gGfx.loadMeshes( );
	gGfx.setThreadPool( &threadPool );

	auto cubes = std::vector<std::vector<std::vector<Object>>>( 8u );
	for ( auto& plane : cubes ) {
		plane.resize( 9u );
		for ( auto& row : plane ) {
			row.resize( 9u );
		}
	}

	for ( std::size_t i = 0u; i < cubes.size( ); ++i ) {
		for ( std::size_t j = 0u; j < cubes[ i ].size( ); ++j ) {
			for ( std::size_t k = 0u; k < cubes[ i ][ j ].size( ); ++k ) {
				cubes[ i ][ j ][ k ].setMesh( gGfx.cubeMesh( ) );
				cubes[ i ][ j ][ k ].setPos( mu::Vec3(
					( static_cast<int>( k ) - static_cast<int>( cubes.size( ) / 2 ) ) * 0.5f,
					( static_cast<int>( j ) - static_cast<int>( cubes.size( ) / 2 ) ) * 0.5f,
					( static_cast<int>( i ) - static_cast<int>( cubes.size( ) / 2 ) ) * 0.5f
				) );
				cubes[ i ][ j ][ k ].setOmega( mu::Vec3( rand( -1.f, 1.f ), rand( -1.f, 1.f ), rand( -1.f, 1.f ) ) );
				cubes[ i ][ j ][ k ].setScale( 0.05f );
			}

		}
	}

	gPlayer = std::make_shared<Object>( );
	gPlayer->setMesh( gGfx.cubeMesh( ) );
	gPlayer->setScale( 0.15f );

	auto dirLight = std::make_shared<Light>( );
	dirLight->setOrient( mu::NQuat( mu::Degree( 0.f ), mu::Degree( -45.f ), mu::Degree( 15.f ) ) );
	dirLight->color = mu::Vec3( 0.8f, 0.8f, 0.8f );
	dirLight->intensity = 1.f;
	dirLight->type = PBRPipeline::LightData::Type::DirectionalLight;

	auto camera = Camera{};
	camera.setTargetObject( gPlayer );
	camera.setOffsetFromTarget( mu::Vec3( 0.f, 0.2f, -0.5f ) );
	camera.setPerspective( mu::Degree( 90.f ),
		static_cast<float>( gWndRect.right - gWndRect.left ) / ( gWndRect.bottom - gWndRect.top ),
		0.025f, 8.f
	);

	Timer timer{};

	auto clientService = std::make_shared<ClientService>(
		NetAddress( serverIp, serverPort ), std::make_shared<IocpCore>( ),
		nullptr, 1 );

	clientService->setSessionFactory( []( ) {
		return std::make_shared<ServerSession>( );
	} );

	ASSERT_CRASH( clientService->start( ) );
	std::thread t1( [&clientService]( ) {
		while ( true ) {
			clientService->getIocpCore( )->dispatch( );
		}
	} );

	//gServerSession->setPlayer( gPlayer );

	// 윈도우 메시지 루프
	MSG msg;
	while ( true ) {
		while ( PeekMessageA( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
			if ( msg.message == WM_QUIT ) {
				t1.join( );
				SocketUtils::rel( );
				return static_cast<int>( msg.wParam );
			}

			TranslateMessage( &msg );
			DispatchMessageA( &msg );
		}

		timer.tick( );

		for ( auto& plane : cubes ) {
			for ( auto& row : plane ) {
				for ( auto& cube : row ) {
					cube.update( timer.deltaTime<Milliseconds>( ) );
				}
			}
		}

		if ( GetForegroundWindow() == ghWnd && GetAsyncKeyState( 'W' ) & 0x8000 ) {
			auto packet = Packet{
				.header = {
					.size = sizeof( PacketHeader ) + sizeof( CSMovePacket ),
					.id = static_cast<std::uint16_t>( PacketType::csMove )
				},
				.csMove = {
					.dir = direction::w
				}
			};
			
			int32 size = sizeof( Packet );
			auto sendBuffer = std::make_shared<SendBuffer>( size );
			sendBuffer->copyData( &packet, sizeof( Packet ) );
			gServerSession->send( sendBuffer );
			//player->setPos( player->pos( ) + mu::Vec3( 0.f, 0.f, 0.01f ) );
		}
		if ( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState( 'A' ) & 0x8000 ) {
			auto packet = Packet{
				.header = {
					.size = sizeof( PacketHeader ) + sizeof( CSMovePacket ),
					.id = static_cast<std::uint16_t>( PacketType::csMove )
				},
				.csMove = {
					.dir = direction::a
				}
			};

			int32 size = sizeof( Packet );
			auto sendBuffer = std::make_shared<SendBuffer>( size );
			sendBuffer->copyData( &packet, sizeof( Packet ) );
			gServerSession->send( sendBuffer );
			//player->setPos( player->pos( ) + mu::Vec3( -0.01f, 0.f, 0.f ) );
		}
		if ( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState( 'S' ) & 0x8000 ) {
			auto packet = Packet{
				.header = {
					.size = sizeof( PacketHeader ) + sizeof( CSMovePacket ),
					.id = static_cast<std::uint16_t>( PacketType::csMove )
				},
				.csMove = {
					.dir = direction::s
				}
			};

			int32 size = sizeof( Packet );
			auto sendBuffer = std::make_shared<SendBuffer>( size );
			sendBuffer->copyData( &packet, sizeof( Packet ) );
			gServerSession->send( sendBuffer );
			//player->setPos( player->pos( ) + mu::Vec3( 0.f, 0.f, -0.01f ) );
		}
		if ( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState( 'D' ) & 0x8000 ) {
			auto packet = Packet{
				.header = {
					.size = sizeof( PacketHeader ) + sizeof( CSMovePacket ),
					.id = static_cast<std::uint16_t>( PacketType::csMove )
				},
				.csMove = {
					.dir = direction::d
				}
			};

			int32 size = sizeof( Packet );
			auto sendBuffer = std::make_shared<SendBuffer>( size );
			sendBuffer->copyData( &packet, sizeof( Packet ) );
			gServerSession->send( sendBuffer );
			//player->setPos( player->pos( ) + mu::Vec3( 0.01f, 0.f, 0.f ) );
		}

		{
			std::lock_guard<std::mutex> lock( gMtx );
			for ( const auto& [pId, object] : gObjects ) {
				object->update( timer.deltaTime<Milliseconds>( ) );
			}
		}
		
		//player->update( timer.deltaTime<Milliseconds>( ) );
		dirLight->update( timer.deltaTime<Milliseconds>( ) );
		camera.update( );
		camera.updateGFX( gGfx );

		for ( auto& plane : cubes ) {
			for ( auto& row : plane ) {
				for ( auto& cube : row ) {
					cube.render( gGfx );
				}
			}
		}

		for( const auto& [ pId, object ] : gObjects ) {
			object->render( gGfx );
		}
		//player->render( gGfx );
		dirLight->render( gGfx );

		auto title = wndName + "(FPS: "s + std::to_string( timer.fps( ) ) + ")"s;
		SetWindowTextA( ghWnd, title.c_str( ) );

		auto frameData = PBRPipeline::FrameData{
			.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
		};
		gGfx.addFrameData( frameData );

		gGfx.render( );
	}
}

LRESULT wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CLOSE:
		PostMessageA(ghWnd, WM_DESTROY, 0, 0);
		DISPLAY_ERROR_GLE(DestroyWindow(hWnd), true);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	default:
		return DefWindowProcA(hWnd, msg, wParam, lParam);
	}
}