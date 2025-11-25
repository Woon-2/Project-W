#ifndef __spriteAnimation_HPP
#define __spriteAnimation_HPP

#include "pch.hpp"
#include "gfxUtil.hpp"

class GFX;

struct FrameInfo
{
	std::string name;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

enum class AnimType : std::uint32_t
{
	Loop = 0,
	Once = 1,
	SIZE
};

struct AnimationData
{
	std::string name;
	AnimType type = AnimType::Loop;
	std::uint32_t frameCount = 0;
	std::uint32_t frameTimeMs = 0;
	std::vector<FrameInfo> frames;
};

// 바이너리 파일로부터 스프라이트 애니메이션의 정보들을 로드하고 그것을 바탕으로 texAnimHashMap을 채운다.
std::vector<Texture> loadSpritesFromFile(
	const std::filesystem::path& path,
	ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
	std::unordered_map<std::string, std::vector<Texture>>& texAnimHashMap,
	DescriptorPool& texPool, Fence& fenceToAssociate
);

class SpriteAnimation {
public:
	void setTexture( const std::vector<Texture>& textures );
	void update( Milliseconds deltaTime );
	void render( GFX& gfx );

private:
	mu::Mat4x4 world_{};	// GFX에 행렬을 전달할 때만 사용된다.
	Texture pCurrentTexture_;
	std::vector<Texture> sprites_{};

	float frmaeDuration_ = 1000.f / 12.f; // 12fps
	float frameAcc_ = 0.f;
	std::size_t currentFrameIdx_ = 0;
};

#endif // __spriteAnimation_HPP