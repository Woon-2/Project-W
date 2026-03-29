#ifndef __pbrSkinnedPipeline_HPP
#define __pbrSkinnedPipeline_HPP

#include "gfxUtil.hpp"
#include "sharedResources.hpp"

class RootSig;

struct Mesh;
struct SubMesh;
struct Material;

namespace ShadowMapSkinnedShader {
	struct PerInstanceData;
}

namespace PBRSkinnedShader { 
	struct PerInstanceData;
}

namespace PBRSkinnedPipeline {

struct LightData {
	enum class Type {
		PointLight,
		Spotlight,
		DirectionalLight
	};
	mu::Vec3 pos;
	mu::Vec3 dir;
	mu::Vec3 color;
	float intensity;
	float cosTheta;
	float cosPhi;
	float falloff;
	mu::Vec3 atten;
	Type type;
	bool isMainDirectionalLight;
	// CSM cascade VP data (only meaningful when isMainDirectionalLight == true)
	std::array<mu::Mat4x4, MAX_CSM_CASCADES> cascadeViews  = {};
	std::array<mu::Mat4x4, MAX_CSM_CASCADES> cascadeProjs  = {};
	XMFLOAT4 cascadeSplitsFarV = {};  // view-space far depth per cascade
	u32t cascadeCount = MAX_CSM_CASCADES;
	std::array<float, MAX_CSM_CASCADES> cascadeNormalOffsets = {};
};

struct CameraData {
	mu::Mat4x4 view;
	mu::Mat4x4 proj;
	mu::Vec3 pos;
};

struct FrameData {
	mu::Vec3 globalAmbient;
};

struct DrawEvent {
	mu::Mat4x4 world;
	std::span<mu::Mat4x4> boneXforms;
	const Mesh* mesh;
	const SubMesh* subMesh;
	const Material* material;

	// 이 함수로 인해 DrawEvent 정렬 시
	// 같은 메시를 공유하는 DrawEvent들끼리 1차적,
	// 같은 서브메시를 공유하는 DrawEvent들끼리 2차적,
	// 같은 재질을 공유하는 DrawEvent들끼리 3차적으로 모이게 된다.
	// 이는 인스턴싱에 용이하다.
	auto operator<=>(const DrawEvent& rhs) const noexcept {
		auto e = mesh <=> rhs.mesh;
		if ( e == std::strong_ordering::equal ) {
			auto e2 = subMesh <=> rhs.subMesh;
			if (e2 == std::strong_ordering::equal) {
				return material <=> rhs.material;
			}
			return e2;
		}
		return e;
	}
};

struct Resources {
	struct ShadowPass {
		StructuredBuffer    perInstanceData;   // t0
		StructuredBuffer    boneData;          // t2
		ConstantBufferArray perDrawcallData;   // b0
		ConstantBufferArray perFrameData;      // b1: cascade별 별도 슬롯 (MAX_CSM_CASCADES)
	} shadowPass;

	struct MainPass {
		StructuredBuffer perInstanceData;	// t0
		StructuredBuffer lightData;	// t1
		StructuredBuffer boneData;	// t2
		ConstantBufferArray perDrawcallData;	// b0
		ConstantBuffer perFrameData;	// b1
	} mainPass;
};

// PBR-skinned Pipeline의 input layout을 위한 Vertex Buffer View 배열이
// mesh에 존재하지 않는다면, 추가한다.
// 0: position, 1: normal, 2: tangent, 3: bitangent, 4: uv, 5: boneIndices, 6: boneWeights
void layoutMeshIfNeeded(const Mesh& mesh);

// PBR-skinned Pipeline의 Dispatcher
// Dispatcher 클래스는 GFX에서 필요한 인자들을 받아
// 파이프라인의 특정 단계를 싱글스레드 혹은 멀티스레드로 수행한다.
// 몇 개의 함수에서 공유하는 데이터들을 따로 모아 보관하는 동시에
// 멀티스레드 작업 분배 과정을 좀 더 쉽게 작성하기 위해 만들어졌다.
// 
// PBR-skinned Pipeline은 H/W 인스턴싱을 상정하므로,
// 미리 draw event들을 그룹화(정렬)할 필요가 있다.
// Dispatcher::sortDrawEvents 함수를 제일 먼저 호출하도록 하자.
//
// PBR-skinned Pipeline은 2-pass 렌더링이다.
// - shadow pass
// - main pass
// shadow pass를 먼저 수행한 뒤 main pass를 수행하도록 한다.
class Dispatcher {
public:
	Dispatcher() = default;
	// GFX 객체로부터 필요한 인자들을 전달받자.
	Dispatcher(
		const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
		DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
		DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
		DescriptorPool* pCmpSamPool, DescriptorPool* pDsvPool,
		const std::shared_ptr<RootSig>& rootSig,
		const ComPtr<ID3D12PipelineState>& mainShader,
		const ComPtr<ID3D12PipelineState>& shadowShader,
		const ComPtr<ID3D12CommandQueue>& cmdQ,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
		D3D12_CPU_DESCRIPTOR_HANDLE dsv, Fence* pFence,
		Resources* pResources, ThreadPool* threadPool,
		CommandListPool* commandListPool,
		std::vector<DrawEvent>&& drawEvent,
		std::vector<LightData>&& lightData,
		const LightData& mainDirectionalLightData,
		const CameraData& cameraData, const FrameData& frameData,
		std::size_t roomIdx
	);

	// draw event들을 H/W 인스턴싱을 위해 그룹화(정렬)한다.
	// 가장 먼저 호출되어야 한다.
	void sortDrawEvents();
	// shadow pass를 싱글 스레드로 수행한다.
	// 1번째 렌더패스에 해당한다.
	void shadowPass();
	// shadow pass를 멀티 스레드로 수행한다.
	// 1번째 렌더패스에 해당한다.
	void shadowPassMT();
	// main pass를 싱글 스레드로 수행한다.
	// 2번째 렌더패스에 해당한다.
	void mainPass();
	// main pass를 멀티 스레드로 수행한다.
	// 2번째 렌더패스에 해당한다.
	void mainPassMT();

private:
	// shadow pass에서 사용하는 GPU 데이터를 갱신한다.
	// DrawEvents, CameraData, LightData에 담겨있는 정보를 가공하여
	// Resources 객체에 담긴, ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
	// 싱글스레드로 동작한다.
	// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
	void shadowUpdate();
	// shadow pass에서 사용하는 GPU 데이터를 갱신한다.
	// DrawEvents, CameraData, LightData에 담겨있는 정보를 가공하여
	// Resources 객체에 담긴, ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
	// 멀티스레드로 동작한다.
	// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
	void shadowUpdateMT();
	void shadowDraw();
	void shadowDrawMT();

	// main pass에서 사용하는 GPU 데이터를 갱신한다.
	// DrawEvents, CameraData, LightData, FrameData에 담겨있는 정보를 가공하여
	// Resources 객체에 담긴, ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
	// 싱글스레드로 동작한다.
	// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
	void mainUpdate();
	// main pass에서 사용하는 GPU 데이터를 갱신한다.
	// DrawEvents, CameraData, LightData, FrameData에 담겨있는 정보를 가공하여
	// Resources 객체에 담긴, ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
	// 멀티스레드로 동작한다.
	// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
	void mainUpdateMT();
	// DrawEvents의 정보들을 참고하여
	// 드로우콜들을 수행한다.
	// 싱글스레드로 동작한다.
	// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
	void mainDraw();
	// DrawEvents의 정보들을 참고하여
	// 드로우콜들을 수행한다.
	// 멀티스레드로 동작한다.
	// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
	void mainDrawMT();

	// 멀티스레드 작업 시, GPU 데이터 갱신 작업에 대해
	// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
	void MU_CALLCONV addJobMainUpdate( mu::Mat4x4 view, const mu::Mat4x4& viewProj,
		const DrawEvent* pFirst, const DrawEvent* pLast, PBRSkinnedShader::PerInstanceData* pOut,
		std::latch& latch
	);
	// 멀티스레드 작업 시, 드로우콜들에 대해
	// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
	void addJobMainDraw( ID3D12GraphicsCommandList* threadCmdList,
		const std::vector<DrawEvent>::const_iterator* pItFirst,
		const std::vector<DrawEvent>::const_iterator* pItLast,
		std::size_t firstDrawcallIdx, std::latch& latch
	);
	// 멀티스레드 작업 시, GPU 데이터 갱신 작업에 대해
	// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
	void addJobShadowUpdate( const DrawEvent* pFirst,
		const DrawEvent* pLast, ShadowMapSkinnedShader::PerInstanceData* pOut,
		std::latch& latch
	);
	// 멀티스레드 작업 시, 드로우콜들에 대해
	// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
	void addJobShadowDraw( ID3D12GraphicsCommandList* threadCmdList,
		const std::vector<DrawEvent>::const_iterator* pItFirst,
		const std::vector<DrawEvent>::const_iterator* pItLast,
		std::size_t firstDrawcallIdx, const CSMShadowMapData::CascadeSlice& cascadeSlice,
		u32t cascadeIdx, std::latch& latch
	);

	// GFX로부터 전달되어 그대로 사용하는 변수들
	std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
	DescriptorPool* pTexPool_ = nullptr;
	DescriptorPool* pTexArrayPool_ = nullptr;
	DescriptorPool* pTexCubePool_ = nullptr;
	DescriptorPool* pSamPool_ = nullptr;
	DescriptorPool* pCmpSamPool_ = nullptr;
	DescriptorPool* pDsvPool_ = nullptr;
	std::shared_ptr<RootSig> rootSig_ = nullptr;
	ComPtr<ID3D12PipelineState> mainShader_ = nullptr;
	ComPtr<ID3D12PipelineState> shadowShader_ = nullptr;
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
	std::vector<LightData> lightData_{};
	LightData mainDirectionalLightData_{};
	CameraData cameraData_{};
	FrameData frameData_{};
	std::size_t roomIdx_{};
	
	// GFX로부터 전달된 것들은 통해 얻어지는 변수들
	UINT rootParamIdxPID_{};
	UINT rootParamIdxPDD_{};
	UINT rootParamIdxPFD_{};
	UINT rootParamIdxLightData_{};
	UINT rootParamIdxBoneData_{};
	UINT rootParamIdxTexPool_{};
	UINT rootParamIdxTexArrayPool_{};
	UINT rootParamIdxTexCubePool_{};
	UINT rootParamIdxSamPool_{};
	UINT rootParamIdxCmpSamPool_{};

	// 멀티스레드 동작 시 작업 카테고리별 분배 단위
	std::size_t jobSizeUpdate_ = 120u;
	std::size_t jobSizeDraw_ = 200u;
};

}	// namespace PBRSkinnedPipeline

#endif	// __pbrSkinnedPipeline_HPP