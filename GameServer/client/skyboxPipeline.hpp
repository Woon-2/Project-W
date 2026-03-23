#ifndef __skyboxPipeline_HPP
#define __skyboxPipeline_HPP

#include "gfxUtil.hpp"

class RootSig;

struct Mesh;
struct SubMesh;

namespace SkyboxPipeline {

struct CameraData {
	mu::Mat4x4 view;
	mu::Mat4x4 proj;
};

struct DrawEvent {
	const Mesh* mesh;
	const SubMesh* subMesh;
	const Texture* texSkybox;
};

struct Resources {
	ConstantBufferArray perDrawcallData;	// b0
	ConstantBuffer perFrameData;	// b1
};

// Skybox Pipeline의 input layout을 위한 Vertex Buffer View 배열이
// mesh에 존재하지 않는다면, 추가한다.
// 0: position
void layoutMeshIfNeeded(const Mesh& mesh);

// PBR Pipeline의 Dispatcher
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
		const ComPtr<ID3D12CommandQueue>& cmdQ,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
		D3D12_CPU_DESCRIPTOR_HANDLE dsv, Fence* pFence,
		Resources* pResources,
		CommandListPool* commandListPool,
		std::vector<DrawEvent>&& drawEvent,
		const CameraData& cameraData,
		std::size_t roomIdx
	);

	// 셰이더에서 사용하는 GPU 데이터를 갱신한다.
	// DrawEvents, CameraData, LightData, FrameData에 담겨있는 정보를 가공하여
	// Resources 객체에 담긴, ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
	// 싱글스레드로 동작한다.
	// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
	void updateGPUDataSingleThreaded();
	// DrawEvents의 정보들을 참고하여
	// 드로우콜들을 수행한다.
	// 싱글스레드로 동작한다.
	// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
	void drawSingleThreaded();

private:

	// GFX로부터 전달되어 그대로 사용하는 변수들
	std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
	DescriptorPool* pTexPool_ = nullptr;
	DescriptorPool* pTexArrayPool_ = nullptr;
	DescriptorPool* pTexCubePool_ = nullptr;
	DescriptorPool* pSamPool_ = nullptr;
	DescriptorPool* pCmpSamPool_ = nullptr;
	std::shared_ptr<RootSig> rootSig_ = nullptr;
	ComPtr<ID3D12PipelineState> shader_ = nullptr;
	ComPtr<ID3D12CommandQueue> cmdQ_ = nullptr;
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT scissorRect_{};
	D3D12_CPU_DESCRIPTOR_HANDLE rtv_{};
	D3D12_CPU_DESCRIPTOR_HANDLE dsv_{};
	Fence* pFence_{};
	CommandListPool* cmdListPool_ = nullptr;
	Resources* pResources_ = nullptr;
	std::vector<DrawEvent> drawEvents_{};
	CameraData cameraData_{};
	std::size_t roomIdx_{};
	
	// GFX로부터 전달된 것들은 통해 얻어지는 변수들
	UINT rootParamIdxPDD_{};
	UINT rootParamIdxPFD_{};
	UINT rootParamIdxLightData_{};
	UINT rootParamIdxTexPool_{};
	UINT rootParamIdxTexArrayPool_{};
	UINT rootParamIdxTexCubePool_{};
	UINT rootParamIdxSamPool_{};
	UINT rootParamIdxCmpSamPool_{};
};

}	// namespace SkyboxPipeline

#endif	// __skyboxPipeline_HPP