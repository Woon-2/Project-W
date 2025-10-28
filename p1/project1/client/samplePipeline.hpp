#ifndef __samplePipeline_HPP
#define __samplePipeline_HPP

#include "pch.hpp"
#include "gfxUtil.hpp"

class RootSig;

struct Mesh;
struct SubMesh;
namespace SampleShader { 
	struct PerInstanceData;
}

// 렌더링 파이프라인별 구조체 -------------------------
namespace SamplePipeline {

struct CameraData {
	mu::Mat4x4 view;
	mu::Mat4x4 proj;
};

struct DrawEvent {
	mu::Mat4x4 world;
	const Mesh* mesh;
	const SubMesh* subMesh;

	// 이 함수로 인해 DrawEvent 정렬 시
	// 같은 메시를 공유하는 DrawEvent들끼리 1차적,
	// 같은 서브메시를 공유하는 DrawEvent들끼리 2차적으로 모이게 된다.
	// 이는 인스턴싱에 용이하다.
	auto operator<=>(const DrawEvent& rhs) const noexcept {
		auto e = mesh <=> rhs.mesh;
		if ( (mesh <=> rhs.mesh) == std::strong_ordering::equal ) {
			return subMesh <=> rhs.subMesh;
		}
		return e;
	}
};

struct Resources {
	StructuredBuffer perInstanceData;
	ConstantBufferArray perDrawcallData;
};

// SamplePipeline의 Dispatcher
// Dispatcher 클래스는 GFX에서 필요한 인자들을 받아
// 파이프라인의 특정 단계를 싱글스레드 혹은 멀티스레드로 수행한다.
// 몇 개의 함수에서 공유하는 데이터들을 따로 모아 보관하는 동시에
// 멀티스레드 작업 분배 과정을 좀 더 쉽게 작성하기 위해 만들어졌다.
class Dispatcher {
public:
	Dispatcher() = default;
	// GFX 객체로부터 필요한 인자들을 전달받자.
	Dispatcher(const std::shared_ptr<RootSig>& rootSig,
		const ComPtr<ID3D12PipelineState>& shader,
		const ComPtr<ID3D12CommandQueue>& cmdQ,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
		D3D12_CPU_DESCRIPTOR_HANDLE dsv, Fence* pFence,
		Resources* pResources, ThreadPool* threadPool,
		CommandListPool* commandListPool,
		std::vector<DrawEvent>&& drawEvent,
		const CameraData& cameraData, std::size_t roomIdx
	);

	// 셰이더에서 사용하는 GPU 데이터를 갱신한다.
	// DrawEvents에 담겨있는 정보를 가공하여
	// Resources 객체에 담긴,
	// ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
	// 싱글스레드로 동작한다.
	void updateGPUDataSingleThreaded();
	// 셰이더에서 사용하는 GPU 데이터를 갱신한다.
	// DrawEvents에 담겨있는 정보를 가공하여
	// Resources 객체에 담긴,
	// ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
	// 멀티스레드로 동작한다.
	void updateGPUDataMultiThreaded();
	// DrawEvents의 정보들을 참고하여
	// 드로우콜들을 수행한다.
	// 싱글스레드로 동작한다.
	void drawSingleThreaded();
	// DrawEvents의 정보들을 참고하여
	// 드로우콜들을 수행한다.
	// 멀티스레드로 동작한다.
	void drawMultiThreaded();

private:
	// 멀티스레드 작업 시, GPU 데이터 갱신 작업에 대해
	// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
	void MU_CALLCONV addJobUpdate( mu::Mat4x4 viewProj, const DrawEvent* pFirst,
		const DrawEvent* pLast, SampleShader::PerInstanceData* pOut, std::latch& latch
	);
	// 멀티스레드 작업 시, 드로우콜들에 대해
	// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
	void addJobDraw( ID3D12GraphicsCommandList* threadCmdList,
		const DrawEvent* pFirst, const DrawEvent* pLast,
		std::size_t firstInstanceIdx, std::latch& latch
	);

	// GFX로부터 전달되어 그대로 사용하는 변수들
	std::shared_ptr<RootSig> rootSig_ = nullptr;
	ComPtr<ID3D12PipelineState> shader_ = nullptr;
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
	std::size_t roomIdx_{};
	
	// GFX로부터 전달된 것들은 통해 얻어지는 변수들
	UINT rootParamIdxPID_{};
	UINT rootParamIdxPDD_{};

	// 멀티스레드 동작 시 작업 카테고리별 분배 단위
	std::size_t jobSizeUpdate_ = 4000u;
	std::size_t jobSizeDraw_ = 200u;
};

}	// namespace SamplePipeline

#endif	// __samplePipeline_HPP