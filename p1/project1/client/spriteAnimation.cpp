#include "spriteAnimation.hpp"
#include "gfx.hpp"
#include "errorHandling.hpp"
#include "binaryImport.hpp"

void SpriteAnimation::setTexture( const std::vector<Texture>& textures )
{
	sprites_ = textures; 
	pCurrentTexture_ = sprites_[currentFrameIdx_];
}

void SpriteAnimation::update( Milliseconds deltaTime ) {
	// 24fps로 애니메이션 재생
	frameAcc_ += deltaTime.count();
	if ( frameAcc_ >= frmaeDuration_ ) {
		frameAcc_ -= frmaeDuration_;
		// 다음 프레임으로 전환
		currentFrameIdx_ = (currentFrameIdx_ + 1) % sprites_.size();
	}
	pCurrentTexture_ = sprites_[currentFrameIdx_];
}

void SpriteAnimation::render( GFX& gfx ) {
	gfx.addDrawEvent( BillboardPipeline::DrawEvent{
		.world = world_,
		.pTex = &pCurrentTexture_
		} );
	
}

std::vector<Texture> loadSpritesFromFile( const std::filesystem::path& path, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, 
	std::unordered_map<std::string, std::vector<Texture>>& texAnimHashMap, DescriptorPool& texPool, Fence& fenceToAssociate )
{
	std::vector<Texture> ret{};
	AnimationData animData{};

	auto ifs = std::ifstream( path, std::ios::binary );
	DISPLAY_ERROR_STR( ifs.good(), "[File I/O Error]: loadSpritesFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, false );
	if ( !ifs ) {
		return ret;
	}

	readHeadTag( ifs, "Name" );
	animData.name = readString( ifs );
	readTailTag( ifs, "Name" );

	readHeadTag( ifs, "Type" );
	animData.type = static_cast<AnimType>(readInteger( ifs ));
	readTailTag( ifs, "Type" );

	readHeadTag( ifs, "FrameCount" );
	animData.frameCount = static_cast<std::uint32_t>(readInteger( ifs ));
	readTailTag( ifs, "FrameCount" );

	readHeadTag( ifs, "FrameTime" );
	animData.frameTimeMs = static_cast<std::uint32_t>(readInteger( ifs ));
	readTailTag( ifs, "FrameTime" );

	readHeadTag( ifs, "Frames" );
	for ( std::uint32_t i = 0; i < animData.frameCount; ++i ) {
		FrameInfo frameInfo{};
		readHeadTag( ifs, "Frame" );
		//
		readHeadTag( ifs, "Name" );
		frameInfo.name = readString( ifs );
		readTailTag( ifs, "Name" );

		readHeadTag( ifs, "Width" );
		frameInfo.width = static_cast<std::uint32_t>(readInteger( ifs ));
		readTailTag( ifs, "Width" );

		readHeadTag( ifs, "Height" );
		frameInfo.height = static_cast<std::uint32_t>(readInteger( ifs ));
		readTailTag( ifs, "Height" );

		animData.frames.push_back( frameInfo );
		//
		readTailTag( ifs, "Frame" );
		// 텍스처 로드
		std::string texturePath = "../resources/Sprites/" + animData.name + '/' + frameInfo.name;
		Texture::Type type{};
		auto texture = loadTexture( device, cmdList, texturePath, fenceToAssociate, type );
		createSRV( device, texture, texPool );
		texture.idxSrv.idxSampler = etoi( Samplers::TrilinearWrap );
		ret.push_back( texture );
	}
	readTailTag( ifs, "Frames" );

	texAnimHashMap.try_emplace( animData.name, ret );

	gSharedLog << "[Resource Load] File I/O: Sprites " << '(' << path << ") 로드 완료\n";

	return ret;
}

