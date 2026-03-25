#ifndef __billboardPipeline_HPP
#define __billboardPipeline_HPP

#include "gfxUtil.hpp"

class RootSig;

struct Mesh;
struct SubMesh;
struct Material;

namespace BillboardShader { 
	struct PerInstanceData;
}

namespace BillboardPipeline {

void initStaticPointMesh( ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, Fence& fenceToAssociate );

struct CameraData {
	mu::Mat4x4 view;
	mu::Mat4x4 proj;
	mu::Vec3 pos;
};

struct FrameData {
	mu::Mat4x4 vp;
	mu::Vec3 cameraPosV;
	float padding0;
};

struct DrawEvent {
	mu::Mat4x4 world;
	const Texture* pTex;
	mu::Vec2 uvOffset = mu::Vec2(0.f, 0.f);	// 스프라이트 시트 내 현재 프레임의 좌상단 UV
	mu::Vec2 uvScale  = mu::Vec2(1.f, 1.f);	// 현재 프레임의 UV 크기 (= 1/cols, 1/rows)
	mu::Vec3 tint;
	bool additive = false;
	float rotation = 0.f;
	int renderOrder = 0;	// 낮을수록 먼저 렌더 (Unity Order in Layer 동일 개념)

	// Non-additive events sort before additive so we can switch PSO once.
	// Within the same blend mode, sort by renderOrder (ascending).
	auto operator<=>( const DrawEvent& rhs ) const noexcept {
		if ( additive != rhs.additive )
			return additive ? std::strong_ordering::greater : std::strong_ordering::less;
		return renderOrder <=> rhs.renderOrder;
	}
};

struct Resources {
	StructuredBuffer perInstanceData;		// t0
	ConstantBufferArray perDrawcallData;	// b0
	ConstantBuffer perFrameData;			// b1
};

// Billboard Pipeline의 Dispatcher
// Dispatcher 클래스는 GFX에서 필요한 인자들을 받아
// 파이프라인의 특정 단계를 싱글스레드 혹은 멀티스레드로 수행한다.
// 몇 개의 함수에서 공유하는 데이터들을 따로 모아 보관하는 동시에
// 멀티스레드 작업 분배 과정을 좀 더 쉽게 작성하기 위해 만들어졌다.
class Dispatcher {
public:
	Dispatcher() = default;
	// GFX 객체로부터 필요한 인자들을 전달받자.
	Dispatcher(
		const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
		DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
		DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
		DescriptorPool* pCmpSamPool,
		const std::shared_ptr<RootSig>& rootSig,
		const ComPtr<ID3D12PipelineState>& shader,
		const ComPtr<ID3D12PipelineState>& shaderAdditive,
		const ComPtr<ID3D12CommandQueue>& cmdQ,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
		D3D12_CPU_DESCRIPTOR_HANDLE dsv, Fence* pFence,
		Resources* pResources, ThreadPool* threadPool,
		CommandListPool* commandListPool,
		std::vector<DrawEvent>&& drawEvents,
		const CameraData& cameraData, const FrameData& frameData,
		std::size_t roomIdx
	);

	// 셰이더에서 사용하는 GPU 데이터를 갱신한다.
	// DrawEvents, CameraData에 담겨있는 정보를 가공하여
	// Resources 객체에 담긴,
	// ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
	// 싱글스레드로 동작한다.
	// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
	void updateGPUDataSingleThreaded();
	// 셰이더에서 사용하는 GPU 데이터를 갱신한다.
	// DrawEvents, CameraData에 담겨있는 정보를 가공하여
	// Resources 객체에 담긴,
	// ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
	// 멀티스레드로 동작한다.
	// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
	void updateGPUDataMultiThreaded();
	// DrawEvents의 정보들을 참고하여
	// 드로우콜들을 수행한다.
	// 싱글스레드로 동작한다.
	// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
	void drawSingleThreaded();
	// DrawEvents의 정보들을 참고하여
	// 드로우콜들을 수행한다.
	// 멀티스레드로 동작한다.
	// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
	void drawMultiThreaded();

private:
	// 멀티스레드 작업 시, GPU 데이터 갱신 작업에 대해
	// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
	void MU_CALLCONV addJobUpdate( mu::Mat4x4 viewProj, const DrawEvent* pFirst,
		const DrawEvent* pLast, BillboardShader::PerInstanceData* pOut, std::latch& latch
	);
	// 멀티스레드 작업 시, 드로우콜들에 대해
	// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
	void addJobDraw( ID3D12GraphicsCommandList* threadCmdList,
		const DrawEvent* pFirst, const DrawEvent* pLast,
		std::size_t firstInstanceOffset, std::latch& latch,
		ID3D12PipelineState* pso
	);

	std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
	DescriptorPool* pTexPool_ = nullptr;
	DescriptorPool* pTexArrayPool_ = nullptr;
	DescriptorPool* pTexCubePool_ = nullptr;
	DescriptorPool* pSamPool_ = nullptr;
	DescriptorPool* pCmpSamPool_ = nullptr;
	std::shared_ptr<RootSig> rootSig_ = nullptr;
	ComPtr<ID3D12PipelineState> shader_ = nullptr;
	ComPtr<ID3D12PipelineState> shaderAdditive_ = nullptr;
	ComPtr<ID3D12CommandQueue> cmdQ_ = nullptr;
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT scissorRect_{};
	D3D12_CPU_DESCRIPTOR_HANDLE rtv_{};
	D3D12_CPU_DESCRIPTOR_HANDLE dsv_{};
	Fence* pFence_{};
	ThreadPool* threadPool_ = nullptr;
	CommandListPool* cmdListPool_ = nullptr;
	Resources* pResources_ = nullptr;
	std::vector<DrawEvent> drawEvents_{};
	CameraData cameraData_{};
	FrameData frameData_{};
	std::size_t roomIdx_{};

	// GFX로부터 전달된 것들을 통해 얻어지는 변수들
	UINT rootParamIdxPDD_{};
	UINT rootParamIdxPID_{};
	UINT rootParamIdxPFD_{};
	UINT rootParamIdxTexPool_{};
	UINT rootParamIdxTexArrayPool_{};
	UINT rootParamIdxTexCubePool_{};
	UINT rootParamIdxSamPool_{};
	UINT rootParamIdxCmpSamPool_{};

	// 멀티스레드 동작 시 작업 카테고리별 분배 단위
	std::size_t jobSizeUpdate_ = 4000u;
	std::size_t jobSizeDraw_ = 200u;
};

}	// namespace BillboardPipeline

namespace Detail {
	extern std::vector<ComPtr<ID3D12Resource>> staticVBPoints;
	extern std::vector<D3D12_VERTEX_BUFFER_VIEW> staticVBViewPoints;
	extern std::map<std::string, u32t> staticVBIdxMapPoints;
	extern ComPtr<ID3D12Resource> staticIBPoint;
	extern D3D12_INDEX_BUFFER_VIEW staticIBViewPoint;
}	// namespace BillboardPipeline::Detail

#endif	// __billboardPipeline_HPP