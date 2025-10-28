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

struct DrawEvent {
	mu::Mat4x4 world;
	const Mesh* mesh;
	const SubMesh* subMesh;

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

// 메인 커맨드 리스트 쓰지 말고 할당받아서 쓰기

// 루트 시그니처, 셰이더, (파이프라인별 이벤트, 파이프라인별 리소스: 멤버 변수) 받아서 초기화
// updateGPUDataSingleThreaded
// updateGPUDataMultiThreaded
// drawSingleThreaded
// drawMultiThreaded
// 기본: 200개 단위로 커맨드리스트 할당, latch 설정
// drawcall 전부 분배 후 latch 기다리기, execute
// 이후: instancing으로 같은 종류 메시 한 번의 드로우콜로 기록할 것 대비
//		Close된 커맨드리스트 쌓아놓고 메인 스레드는 커맨드리스트 개수 체크 후
//      적절한 때 execute -> 스레드별 완료된 커맨드리스트 모음집이 있고,
//		메인 스레드는 각 모음집 각 칸의 atomic_flag를 체크하여 커맨드리스트를 가져온다.
//      그러고 실행: latch를 대체

class Dispatcher {
public:
	Dispatcher() = default;
	Dispatcher(const std::shared_ptr<RootSig>& rootSig,
		const ComPtr<ID3D12PipelineState>& shader,
		const ComPtr<ID3D12CommandQueue>& cmdQ,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
		D3D12_CPU_DESCRIPTOR_HANDLE dsv, Fence* pFence,
		Resources* pResources, ThreadPool* threadPool,
		CommandListPool* commandListPool,
		std::vector<DrawEvent>&& drawEvent, std::size_t roomIdx
	);

	void updateGPUDataSingleThreaded();
	void updateGPUDataMultiThreaded();
	void drawSingleThreaded();
	void drawMultiThreaded();

private:
	void addJobUpdate( const DrawEvent* pFirst, const DrawEvent* pLast,
		SampleShader::PerInstanceData* pOut, std::latch& latch
	);
	void addJobDraw( ID3D12GraphicsCommandList* threadCmdList,
		const DrawEvent* pFirst, const DrawEvent* pLast,
		std::size_t firstInstanceIdx, std::latch& latch
	);

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
	std::size_t roomIdx_{};
	
	UINT rootParamIdxPID_{};
	UINT rootParamIdxPDD_{};

	std::size_t jobSizeUpdate_ = 4000u;
	std::size_t jobSizeDraw_ = 200u;
};

}	// namespace SamplePipeline

#endif	// __samplePipeline_HPP