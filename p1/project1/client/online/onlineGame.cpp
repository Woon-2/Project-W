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
	gfx_.setupDXGI(D3D_FEATURE_LEVEL_12_1);
	gfx_.init();
	gfx_.createSwapChain();
	gfx_.setThreadPool(&threadPool_);

	assetManager_.loadGFXAssets( gfx_ );
}

void Game::setupStage() {
	cubes_.resize( 2u );
	for ( auto& plane : cubes_ ) {
		plane.resize( 2u );
		for ( auto& row : plane ) {
			row.resize( 2u );
		}
	}

	for ( std::size_t i = 0u; i < cubes_.size( ); ++i ) {
		for ( std::size_t j = 0u; j < cubes_[ i ].size( ); ++j ) {
			for ( std::size_t k = 0u; k < cubes_[ i ][ j ].size( ); ++k ) {
				cubes_[ i ][ j ][ k ].setModel( assetManager_.modelCube() );
				cubes_[ i ][ j ][ k ].setPos( mu::Vec3(
					( static_cast<int>( k ) - static_cast<int>( cubes_.size( ) / 2 ) ) * 2.5f,
					( static_cast<int>( j ) - static_cast<int>( cubes_.size( ) / 2 ) ) * 2.5f,
					( static_cast<int>( i ) - static_cast<int>( cubes_.size( ) / 2 ) ) * 2.5f
				) );
				cubes_[ i ][ j ][ k ].setOmega( mu::Vec3( rand( -1.f, 1.f ), rand( -1.f, 1.f ), rand( -1.f, 1.f ) ) );
				cubes_[ i ][ j ][ k ].setScale( 1.f );
			}

		}
	}

	for ( auto& plane : cubes_ ) {
		for ( auto& row : plane ) {
			for ( auto& cube : row ) {
				cube.enableBVRendering( );
			}
		}
	}

	skybox_.setModel( assetManager_.modelCube( ) );
	skybox_.setSkyboxMaterial( assetManager_.skyboxMaterial( ) );

	player_ = std::make_shared<Object>();
	player_->setPos(mu::Vec3(0.f, 0.f, 0.f));
	player_->setModel( assetManager_.modelPlayer());
	player_->setScale(1.f);
	player_->enableBVRendering( );

	dirLight_.setOrient( mu::NQuat( mu::Degree( 0.f ), mu::Degree( 120.f ), mu::Degree( 15.f ) ) );
	dirLight_.color = mu::Vec3( 0.8f, 0.8f, 0.8f );
	dirLight_.intensity = 2.f;
	dirLight_.type = PBRPipeline::LightData::Type::DirectionalLight;

	camera_.setTargetObject( player_ );
	camera_.setOffsetFromTarget( mu::Vec3( 0.f, 1.8f, -2.5f ) );
	camera_.setOffsetTargetPivot( mu::Vec3( 0.f, 1.f, 0.f ) );
	camera_.setPerspective( mu::Degree( 90.f ),
		static_cast<float>( gClientRect.right - gClientRect.left ) / ( gClientRect.bottom - gClientRect.top ),
		0.1f, 500.f
	);
}

void Game::update(Milliseconds deltaTime) {
	processInput(deltaTime);

	// 물리량 갱신은 게임 갱신과 다르게 고정 주기로 수행한다.
	// 이를 통해 너무 유동적인 delta time으로 인한 시뮬레이션의 불안정성과
	// 물리 업데이트의 성능적 비용 문제를 해결한다.
	// 물리 업데이트 주기는 physicUpdateInterval_ 변수에 저장된다.
	//
	// update 함수에서 physicUpdateAcc_ 변수를 통해
	// 물리량 갱신의 주기가 돌아왔는지 판단하고
	// 주기가 되었다면 물리량 갱신을 수행한다.
	physicUpdateAcc_ += deltaTime;

	if ( physicUpdateAcc_ >= physicUpdateInterval ) {
		// 물리 시뮬레이션을 위해
		// 물리 시뮬레이션의 대상이 되는 객체들을
		// 한 곳에 모아 PhysicSystem 객체에 전달한다.
		static std::vector<Object*> allObjects{};
		for ( auto& plane : cubes_ ) {
			for ( auto& row : plane ) {
				for ( auto& cube : row ) {
					allObjects.push_back( &cube );
				}
			}
		}

		allObjects.push_back( player_.get( ) );

		while ( physicUpdateAcc_ >= physicUpdateInterval ) {
			physicSystem_.step( allObjects, physicUpdateInterval );
			physicUpdateAcc_ -= physicUpdateInterval;
		}

		allObjects.clear( );
	}

	// 물리량 갱신 주기에 대해,
	// 마지막 물리량 갱신으로부터 얼마나 지났는지의 비율로
	// RenderState 갱신을 위한 PhysicState 보간 계수를 설정한다.
	// 게임 객체의 update 함수에 전달된다.
	const auto tPhysicInterpolation = physicUpdateAcc_ / physicUpdateInterval;

	for ( auto& plane : cubes_ ) {
		for ( auto& row : plane ) {
			for ( auto& cube : row ) {
				cube.update( deltaTime, tPhysicInterpolation );
			}
		}
	}

	player_->update(deltaTime, tPhysicInterpolation );
	objectsMtx_.lock( );
	for ( auto& obj : otherPlayers_ ) {
		if( obj != player_ ) {
			obj->update( deltaTime, tPhysicInterpolation );
		}
	}
	objectsMtx_.unlock( );

	camera_.update();
	dirLight_.update(deltaTime);
}

void Game::render() {
	for ( auto& plane : cubes_ ) {
		for ( auto& row : plane ) {
			for ( auto& cube : row ) {
				cube.render( gfx_ );
			}
		}
	}

	player_->render(gfx_);
	skybox_.render( gfx_ );
	objectsMtx_.lock( );
	for ( auto& obj : otherPlayers_ ) {
		if( obj != player_ ) {
			obj->render( gfx_ );
		}
	}
	objectsMtx_.unlock( );

	camera_.updateGFX(gfx_);
	dirLight_.render(gfx_);

	auto frameData = PBRPipeline::FrameData{
		.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
	};
	gfx_.addFrameData( frameData );

	gfx_.render();
}

// 윈도우 프로시저에서 특정한 메시지 처리를 위임받는다.
LRESULT Game::receiveWndMsg( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
	return DefWindowProcA( hWnd, msg, wParam, lParam );
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

	bool readyToSend = false;

	if ( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState('W') & 0x8000 ) {
		packet.csMove.dir = Direction::w;
		readyToSend = true;
	}
	if ( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState('A') & 0x8000 ) {
		packet.csMove.dir = Direction::a;
		readyToSend = true;
	}
	if ( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState('S') & 0x8000 ) {
		packet.csMove.dir = Direction::s;
		readyToSend = true;
	}
	if ( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState('D') & 0x8000 ) {
		packet.csMove.dir = Direction::d;
		readyToSend = true;
	}
	/*if ( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState( 'I' ) & 0x8000 ) {
		packet = Packet{
			.header = {
				.size = sizeof( PacketHeader ) + sizeof( CSEnterPacket ),
				.id = static_cast<std::uint16_t>( PacketType::csEnter )
			}
		};
		readyToSend = true;
	}*/
	if ( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState( '1' ) & 0x8000 ) {
		packet = Packet{
			.header = {
				.size = sizeof( PacketHeader ) + sizeof( CSFindRoomPacket ),
				.id = static_cast<std::uint16_t>( PacketType::csFindRoom )
			},
			.csFindRoom = {
				.roomId = 1
			}
		};
		readyToSend = true;
	}
	if( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState( '2' ) & 0x8000 ) {
		packet = Packet{
			.header = {
				.size = sizeof( PacketHeader ) + sizeof( CSFindRoomPacket ),
				.id = static_cast<std::uint16_t>( PacketType::csFindRoom )
			},
			.csFindRoom = {
				.roomId = 2
			}
		};
		readyToSend = true;
	}
	if( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState( '3' ) & 0x8000 ) {
		packet = Packet{
			.header = {
				.size = sizeof( PacketHeader ) + sizeof( CSFindRoomPacket ),
				.id = static_cast<std::uint16_t>( PacketType::csFindRoom )
			},
			.csFindRoom = {
				.roomId = 3
			}
		};
		readyToSend = true;
	}
	if( GetForegroundWindow( ) == ghWnd && GetAsyncKeyState( '4' ) & 0x8000 ) {
		packet = Packet{
			.header = {
				.size = sizeof( PacketHeader ) + sizeof( CSFindRoomPacket ),
				.id = static_cast<std::uint16_t>( PacketType::csFindRoom )
			},
			.csFindRoom = {
				.roomId = 4
			}
		};
		readyToSend = true;
	}

	if( !readyToSend ) {
		return;
	}

	i32t packetSize = sizeof( Packet );
	auto buffer = std::make_shared<SendBuffer>( packetSize );
	buffer->copyData( &packet, packetSize );
	serverSession_->send( buffer );
}

}	// namespace Online