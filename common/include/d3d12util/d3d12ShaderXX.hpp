#ifndef __D3D12ShaderXX_HPP
#define __D3D12ShaderXX_HPP

#include "d3d12util/d3d12Low.hpp"
#include "d3d12util/d3d12ResourceXX.hpp"

#include "gfxExcept.hpp"

#include <bitset>
#include <vector>
#include <ranges>
#include <algorithm>
#include <filesystem>
#include <cstdint>

#include "enumUtil.hpp"

namespace gfx {

namespace d3d12 {

namespace detail {

class UnifiedRootImpl : public RootSignature {
public:
	enum class ParamIndices {
		BindlessTex2D,
		BindlessTexArray,
		BindlessTexCube,
		BindlessSampler,
		BindlessSamplerComparison,
		b0, b1, b2, b3, b4, b5, b6, b7,
		t0, t1, t2, t3, t4, t5, t6, t7,
		u0, u1, u2, u3, u4, u5, u6, u7
	};

	static constexpr auto cbvRegisterCnt = 8u;
	static constexpr auto srvRegisterCnt = 8u;
	static constexpr auto uavRegisterCnt = 8u;

private:
	class Params {
	public:
		UINT operator[](ParamIndices idx) const noexcept {
			return static_cast<UINT>( etoi(idx) );
		}
	};

public:
	UnifiedRootImpl() = default;
	UnifiedRootImpl(D3D12Device& device);

	Params params;
};

} // namespace gfx::d3d12::detail

class UnifiedRoot {
public:
	using ParamIndices = detail::UnifiedRootImpl::ParamIndices;

	static constexpr auto cbvRegisterCnt = detail::UnifiedRootImpl::cbvRegisterCnt;
	static constexpr auto srvRegisterCnt = detail::UnifiedRootImpl::srvRegisterCnt;
	static constexpr auto uavRegisterCnt = detail::UnifiedRootImpl::uavRegisterCnt;

	static void init(D3D12Device& device) {
		impl_ = detail::UnifiedRootImpl(device);
	}

	static detail::UnifiedRootImpl& get() noexcept {
		return impl_;
	}

private:
	static detail::UnifiedRootImpl impl_;
};

class InputLayout {
public:
	enum class Spec {
		serial,
		separated
	};

	struct Elem {
		std::string semanticName;
		UINT semanticIndex;
		DXGI_FORMAT format;
	};

	struct Slot {
		std::vector<Elem> elems;
		std::bitset< etoi(Vertex::Properties::SIZE) > attributes;
	};

	InputLayout() = default;

	template <std::ranges::range R>
		requires std::same_as<std::ranges::range_value_t<R>, Slot>
	InputLayout(R&& slotRange)
		: slots_( std::move_iterator(slotRange.begin()),
			std::move_iterator(slotRange.end())
		) {}

	template <std::ranges::range R>
		requires std::same_as<std::ranges::range_value_t<R>, Slot>
			&& std::is_lvalue_reference_v<R>
	InputLayout(R&& slotRange)
		: slots_(slotRange.begin(), slotRange.end()) {}

	InputLayout(const std::vector<Slot>& slots)
		: slots_(slots) {}

	InputLayout(std::vector<Slot>&& slots)
		: slots_(std::move(slots)) {}

	const Slot& slot(std::size_t idx) const noexcept {
		return slots_[idx];
	}

	std::vector<D3D12_INPUT_ELEMENT_DESC> makeDescs() const {
		auto descs = std::vector<D3D12_INPUT_ELEMENT_DESC>{};
		descs.reserve(32);
		for (std::size_t i = 0; i < slots_.size(); ++i) {
			const auto& slot = slots_[i];
			for (const auto& elem : slot.elems) {
				descs.emplace_back(
					elem.semanticName.c_str(), elem.semanticIndex,
					elem.format, static_cast<UINT>(i), D3D12_APPEND_ALIGNED_ELEMENT
				);
			}
		}
		return descs;
	}

	template <std::ranges::contiguous_range R>
		requires std::same_as<std::ranges::range_value_t<R>, VertexBuffer>
	bool checkBindable(const R& vbs) const {
		if (slots_.size() > std::size(vbs)) {
			return false;
		}
		for (std::size_t i = 0; i < slots_.size(); ++i) {
			const auto& slot = slots_[i];
			if (slot.attributes != vbs[i].attributes()) {
				return false;
			}
		}
		return true;
	}

	std::optional<std::size_t> bindableIdx(const RefMesh& mesh) const;
	std::size_t slotCnt() const noexcept { return slots_.size(); }

private:
	std::vector<Slot> slots_;
};

void arrangeVBs(RefMesh& refMesh, D3D12Device& device, D3D12GfxCmdList& cmdList,
	std::size_t layoutIdx, const InputLayout& inputLayout
);

void arrangeVBs(RefModel& refModel, D3D12Device& device, D3D12GfxCmdList& cmdList,
	std::size_t layoutIdx, const InputLayout& inputLayout
);

class ShaderBlob : public dx::DXWrapper<ID3DBlob> {
public:
	enum class Type {
		Vertex,
		Pixel,
		Geometry,
		Hull,
		Domain,
		Compute,
		Size
	};

	ShaderBlob( const std::filesystem::path& path,
		const D3D_SHADER_MACRO* macros,
		std::string_view entryPoint, std::string_view target,
		UINT flag1, UINT flag2, Type type
	);

	Type type() const noexcept {
		return type_;
	}

private:
	Type type_;
};

class Shader;

class RenderProtocol : public dx::DXWrapper<ID3D12PipelineState> {
public:
	struct Desc {
		D3D12_STREAM_OUTPUT_DESC streamOutput;
		D3D12_BLEND_DESC blend;
		UINT sampleMask;
		D3D12_RASTERIZER_DESC rasterizerState; 
		D3D12_DEPTH_STENCIL_DESC depthStencilState;
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE ibStripCutValue;
		D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType;
		UINT numRenderTargets;
		DXGI_FORMAT rtvFormats[8];
		DXGI_FORMAT dsvFormat;
		DXGI_SAMPLE_DESC sampleDesc;
		UINT nodeMask;
		D3D12_CACHED_PIPELINE_STATE cachedPSO;
		D3D12_PIPELINE_STATE_FLAGS flags;
	};

	template <std::ranges::contiguous_range Blobs>
		requires std::same_as<std::ranges::range_value_t<Blobs>, ShaderBlob>
	RenderProtocol( D3D12Device& device, Shader& shader, const Blobs& blobs,
		const Desc& desc
	) : RenderProtocol(device, shader, blobs.data(), blobs.size(), desc) {}

	RenderProtocol(D3D12Device& device, Shader& shader,
		const ShaderBlob* pBlobs, std::size_t blobCnt, const Desc& desc
	);

	void bind(D3D12GfxCmdList& cmdList) {
		cmdList.get()->SetPipelineState(get().Get());
	}

	std::optional<std::size_t> compatibleLayout(const RefMesh& mesh) const;

	Shader& shader() const noexcept { return *pShader_; }

private:
	Shader* pShader_;
};

class ComputeShader;

class ComputeProtocol : public dx::DXWrapper<ID3D12PipelineState> {
public:
	struct Desc {
		UINT nodeMask;
		D3D12_CACHED_PIPELINE_STATE cachedPSO;
		D3D12_PIPELINE_STATE_FLAGS flags;
	};

	ComputeProtocol(D3D12Device& device, ComputeShader& shader,
		const ShaderBlob& blob, const Desc& desc
	);

	void bind(D3D12GfxCmdList& cmdList) {
		cmdList.get()->SetPipelineState(get().Get());
	}

	ComputeShader& shader() const noexcept { return *pShader_; }

private:
	ComputeShader* pShader_;
};

class Shader {
protected:
	static std::size_t calcConstantBufferSize(std::size_t size) {
		return (size + 255ull) & ~255ull;
	}

public:
	Shader(const RootSignature& root)
		: blobs_(etoi(ShaderBlob::Type::Size)), inputLayout_(), root_(root) {}

	Shader(const RootSignature& root, const InputLayout& il)
		: blobs_(etoi(ShaderBlob::Type::Size)),
		inputLayout_(il), root_(root) {}

	Shader(const RootSignature& root, InputLayout&& il)
		: blobs_(etoi(ShaderBlob::Type::Size)),
		inputLayout_(std::move(il)), root_(root) {}

	virtual ~Shader() = default;

	const auto& blobs() const noexcept {
		return blobs_;
	}

	void loadBlobsIfNot() {
		if (std::ranges::none_of(blobs_, [](const auto& blob) { return blob.has_value(); })) {
			loadBlobs();
		}
	}

	virtual void bindRootParams(D3D12GfxCmdList& cmdList) = 0;

	virtual void loadBlobs() = 0;
	virtual void releaseBlobs() = 0;

	template <ShaderBlob::Type ... Types>
	std::vector<ShaderBlob> selectBlobs() const {
		auto selected = std::vector<ShaderBlob>{};
        auto throwIfNot = [](bool cond) {
            if (!cond) {
                throw GFX_EXCEPT("[Description] required shader blob not found.");
            }
        };
		(throwIfNot(blobs_[etoi(Types)].has_value()), ...);
		(selected.push_back(blobs_[etoi(Types)].value()), ...);
		return selected;
	}

	template <ShaderBlob::Type ... Types>
	std::vector<ShaderBlob> selectBlobsStrong() {
		loadBlobsIfNot();
		return selectBlobs<Types...>();
	}

	void setInputLayout(const InputLayout& inputLayout) {
		inputLayout_ = inputLayout;
	}

	const InputLayout& inputLayout() const noexcept {
		return inputLayout_;
	}

	const RootSignature& rootSiganture() const noexcept {
		return root_;
	}

	void draw( D3D12GfxCmdList& cmdList, const Submesh& submesh,
		std::size_t instanceCnt, std::size_t vbLayoutIdx
	) {
		submesh.draw(cmdList, instanceCnt, vbLayoutIdx);
	}

protected:
	std::vector<std::optional<ShaderBlob>> blobs_;
	InputLayout inputLayout_;
	RootSignature root_;
};


inline std::optional<std::size_t> RenderProtocol::compatibleLayout(
	const RefMesh& mesh
) const {
    return pShader_->inputLayout().bindableIdx(mesh);
}

class ComputeShader {
protected:
	static std::size_t calcConstantBufferSize(std::size_t size) {
		return (size + 255ull) & ~255ull;
	}

public:
	ComputeShader(const RootSignature& root)
		: blob_(), root_(root) {}

	virtual ~ComputeShader() = default;

	const auto& blob() const noexcept {
		return blob_;
	}

	void loadBlobIfNot() {
		if (!blob_.has_value()) {
			loadBlob();
		}
	}

	const RootSignature& rootSiganture() const noexcept {
		return root_;
	}

	virtual void bindRootParams(D3D12GfxCmdList& cmdList) = 0;
	virtual void loadBlob() = 0;
	virtual void releaseBlob() = 0;

	void dispatch( D3D12GfxCmdList& cmdList, std::size_t threadGroupCntX,
		std::size_t threadGroupCntY, std::size_t threadGroupCntZ
	) {
		cmdList.get()->Dispatch(
			static_cast<UINT>(threadGroupCntX),
			static_cast<UINT>(threadGroupCntY),
			static_cast<UINT>(threadGroupCntZ)
		);
	}

protected:
	std::optional<ShaderBlob> blob_;
	RootSignature root_;
};

namespace sr {

struct PerConfigurationData0 {
	float viewportWidth;
	float viewportHeight;
};

struct PerInstanceData0 {
	dx::XMFLOAT4X4 wvp;
	dx::XMFLOAT4X4 world;
	dx::XMFLOAT4X4 wv;
	dx::XMFLOAT3X3 wvNormal;
};

struct PerInstanceData1 {
	dx::XMFLOAT4X4 wv;
	dx::XMFLOAT4X4 proj;
	dx::XMFLOAT3X3 wvNormal;
};

struct PerInstanceData2 {
	dx::XMFLOAT4X4 world;
};

struct PerInstanceData3 {
	dx::XMFLOAT4X4 wvp;
    dx::XMFLOAT4X4 wv;
	dx::XMFLOAT3X3 wvNormal;
};

struct PerInstanceData4 {
	dx::XMFLOAT4X4 world;
	dx::XMFLOAT4X4 wv;
};

struct PerInstanceData5 {
	dx::XMFLOAT4X4 wvp;
	dx::XMFLOAT4X4 world;
	dx::XMFLOAT4X4 wv;
	dx::XMFLOAT3X3 wvNormal;
	std::uint32_t animIdx0;
	std::uint32_t animIdx1;
	float animWeight0;
	float animWeight1;
	int sampleIdx0;
	int sampleIdx1;
	std::uint32_t boneCnt;
	bool usePresampled;
};

struct PerInstanceData6 {
	dx::XMFLOAT4X4 world;
	std::uint32_t animIdx0;
	std::uint32_t animIdx1;
	float animWeight0;
	float animWeight1;
	int sampleIdx0;
	int sampleIdx1;
	std::uint32_t boneCnt;
};

struct Light {
	enum class Type {
		Point,
		Spot,
		Directional,
		Size
	};

	dx::XMFLOAT3 color;
	float falloff;
	dx::XMFLOAT3 posV;
	float cosTheta;
	dx::XMFLOAT3 dirV;
	float cosPhi;
	dx::XMFLOAT3 atten;
	float intensity;
	int type;
	int padding[3];
};

struct PBRMaterial {
	static PBRMaterial convert(const Material& material);

	dx::XMFLOAT4 albedoConstant;
	float roughnessConstant;
	float metallicConstant;
	float albedoConstantMapRatio;
	float roughnessConstantMapRatio;
	float metallicConstantMapRatio;
	dx::XMFLOAT3 emmisiveConstant;
	float emmisiveConstantMapRatio;
	float ambientOcclusionConstant;
	float ambientOcclusionConstantMapRatio;
	float padding;
	dx::XMUINT4 albedoMapRef;
	dx::XMUINT4 roughnessMapRef;
	dx::XMUINT4 normalMapRef;
	dx::XMUINT4 metallicMapRef;
	dx::XMUINT4 metallicSmoothnessMapRef;
	dx::XMUINT4 emmisiveMapRef;
	dx::XMUINT4 ambientOcclusionMapRef;
};

struct PerDrawcallData0 {
	PBRMaterial material;
	std::uint32_t instanceBase;
	std::uint32_t samplerIdx;
	std::uint32_t shadowSamplerIdx;
};

struct PerDrawcallData1 {
	dx::XMFLOAT4X4 wvp;
	dx::XMFLOAT4 color;
};

struct PerDrawcallData2 {
	std::uint32_t instanceBase;
	dx::XMUINT3 padding;
};

struct PerDrawcallData3 {
	dx::XMUINT4 frameMapRef;
	std::uint32_t samplerIdx;
	dx::XMUINT3 padding;
};

struct PerDrawcallData4 {
	PBRMaterial material;
    dx::XMUINT4 heightMapRef;
    std::uint32_t instanceBase;
	std::uint32_t shadowSamplerIdx;
    std::uint32_t heightMapSamplerIdx;
	std::uint32_t samplerIdx;
	dx::XMFLOAT2 tileScale;
    dx::XMFLOAT2 padding;
};

struct PerFrameData0 {
	dx::XMFLOAT3 globalAmbient;
	float padding0;
	dx::XMUINT4 shadowMapRef;
	dx::XMFLOAT4X4 lightVP;
	std::uint32_t lightCnt;	
	dx::XMUINT3 padding1;
};

struct PerFrameData1 {
	dx::XMFLOAT4X4 lightVP;
};

struct PerFrameData2 {
	dx::XMFLOAT3 globalAmbient;
	float padding0;
	dx::XMUINT4 shadowMapRef[3];
	dx::XMFLOAT4X4 lightVP[3];
	std::uint32_t lightCnt;
	dx::XMUINT3 padding1;
};

struct KeyFrame {
	dx::XMFLOAT3 pos;
	float padding;
	dx::XMFLOAT3 scale;
	float padding2;
	dx::XMFLOAT4 rot;
	float ratio;
};

}	// namespace gfx::d3d12::sr

class ShaderPBRIllumination : public Shader {
private:
	std::size_t cbDrawcallDataSize_;
	
public:
	struct Config {
		std::size_t maxInstanceCnt;
		std::size_t maxDrawcallCnt;
		std::size_t maxLightCnt;
	};

	ShaderPBRIllumination( D3D12Device& device, const RootSignature& root,
		const Config& config, InputLayout::Spec ilSpec = InputLayout::Spec::serial
	);

	RenderProtocol makeProtocol( D3D12Device& device, const RenderProtocol::Desc& desc) {
		return RenderProtocol( device, *this,
			selectBlobsStrong<ShaderBlob::Type::Vertex, ShaderBlob::Type::Pixel>(), desc
		);
	}

	std::size_t maxInstanceCnt() const noexcept {
		return maxInstanceCnt_;
	}

	std::size_t maxLightCnt() const noexcept {
		return maxLightCnt_;
	}

	std::size_t maxDrawcallCnt() const noexcept {
		return maxDrawcallCnt_;
	}

	void bindRootParams(D3D12GfxCmdList& cmdList) override;
	void bindPerDrawcallData(std::size_t drawcallIdx, D3D12GfxCmdList& cmdList);

	std::size_t cbDrawcallDataSize() const noexcept {
		return cbDrawcallDataSize_;
	}

	void loadBlobs() override;
	void releaseBlobs() override;

	UploadBuffer perConfigurationData_;
	UploadBuffer perFrameData_;
	UploadBuffer perDrawcallData_;
	UploadBuffer perInstanceData_;
	UploadBuffer lightBuffer_;

private:
	static InputLayout makeInputLayout(InputLayout::Spec ilSpec);
	static InputLayout makeInputLayoutSerial();
	static InputLayout makeInputLayoutSeparated();

	std::size_t maxInstanceCnt_;
	std::size_t maxLightCnt_;
	std::size_t maxDrawcallCnt_;
};

class ShaderPBRAnimatedIllumination : public Shader {
private:
	std::size_t cbDrawcallDataSize_;

public:
	struct Config {
		std::size_t maxInstanceCnt;
		std::size_t maxDrawcallCnt;
		std::size_t maxLightCnt;
		std::size_t maxBoneCnt;
	};

	ShaderPBRAnimatedIllumination( D3D12Device& device, const RootSignature& root,
		const Config& config, InputLayout::Spec ilSpec = InputLayout::Spec::serial
	);

	RenderProtocol makeProtocol( D3D12Device& device, const RenderProtocol::Desc& desc) {
		return RenderProtocol( device, *this,
			selectBlobsStrong<ShaderBlob::Type::Vertex, ShaderBlob::Type::Pixel>(), desc
		);
	}

	std::size_t maxInstanceCnt() const noexcept {
		return maxInstanceCnt_;
	}

	std::size_t maxLightCnt() const noexcept {
		return maxLightCnt_;
	}

	std::size_t maxDrawcallCnt() const noexcept {
		return maxDrawcallCnt_;
	}

	std::size_t maxBoneCnt() const noexcept {
		return maxBoneCnt_;
	}

	void bindRootParams(D3D12GfxCmdList& cmdList) override;
	void bindPerDrawcallData(std::size_t drawcallIdx, D3D12GfxCmdList& cmdList);

	std::size_t cbDrawcallDataSize() const noexcept {
		return cbDrawcallDataSize_;
	}

	void loadBlobs() override;
	void releaseBlobs() override;

	UploadBuffer perConfigurationData_;
	UploadBuffer perFrameData_;
	UploadBuffer perDrawcallData_;
	UploadBuffer perInstanceData_;
	UploadBuffer lightBuffer_;
	UploadBuffer boneBuffer_;

private:
	static InputLayout makeInputLayout(InputLayout::Spec ilSpec);
	static InputLayout makeInputLayoutSerial();
	static InputLayout makeInputLayoutSeparated();

	std::size_t maxInstanceCnt_;
	std::size_t maxLightCnt_;
	std::size_t maxDrawcallCnt_;
	std::size_t maxBoneCnt_;
};

class ShaderPBRIlluminationTerrain : public Shader {
private:
	std::size_t cbDrawcallDataSize_;

public:
	struct Config {
		std::size_t maxInstanceCnt;
		std::size_t maxDrawcallCnt;
		std::size_t maxLightCnt;
	};

	ShaderPBRIlluminationTerrain( D3D12Device& device, const RootSignature& root,
		const Config& config, InputLayout::Spec ilSpec = InputLayout::Spec::serial
	);

	RenderProtocol makeProtocol( D3D12Device& device, const RenderProtocol::Desc& desc) {
		return RenderProtocol( device, *this,
			selectBlobsStrong< ShaderBlob::Type::Vertex, ShaderBlob::Type::Pixel,
				ShaderBlob::Type::Hull, ShaderBlob::Type::Domain
			>(), desc
		);
	}

	std::size_t maxInstanceCnt() const noexcept {
		return maxInstanceCnt_;
	}

	std::size_t maxLightCnt() const noexcept {
		return maxLightCnt_;
	}

	std::size_t maxDrawcallCnt() const noexcept {
		return maxDrawcallCnt_;
	}

	void bindRootParams(D3D12GfxCmdList& cmdList) override;
	void bindPerDrawcallData(std::size_t drawcallIdx, D3D12GfxCmdList& cmdList);

	void loadBlobs() override;
	void releaseBlobs() override;

	UploadBuffer perConfigurationData_;
	UploadBuffer perFrameData_;
	UploadBuffer perDrawcallData_;
	UploadBuffer perInstanceData_;
	UploadBuffer lightBuffer_;

	std::size_t cbDrawcallDataSize() const noexcept {
		return cbDrawcallDataSize_;
	}

private:
	static InputLayout makeInputLayout(InputLayout::Spec ilSpec);
	static InputLayout makeInputLayoutSerial();
	static InputLayout makeInputLayoutSeparated();

	std::size_t maxInstanceCnt_;
	std::size_t maxLightCnt_;
	std::size_t maxDrawcallCnt_;
};

namespace detail
{
const D3D12_SHADER_RESOURCE_VIEW_DESC makeShadowMapSrvDesc(
    const Texture::Desc& shadowMapDesc
);

const D3D12_SHADER_RESOURCE_VIEW_DESC makeShadowMapSrvDesc(
	const D3D12_RESOURCE_DESC& shadowMapDesc
);

const D3D12_DEPTH_STENCIL_VIEW_DESC makeShadowMapDsvDesc(
    const Texture::Desc& shadowMapDesc
);

const D3D12_DEPTH_STENCIL_VIEW_DESC makeShadowMapDsvDesc(
	const D3D12_RESOURCE_DESC& shadowMapDesc
);
}   // namespace gfx::d3d12::detail

class ShaderShadowMap : public Shader {
private:
	std::size_t cbDrawcallDataSize_;

public:
	struct Config {
		std::size_t maxInstanceCnt;
		std::size_t maxDrawcallCnt;
	};

	ShaderShadowMap( D3D12Device& device, const RootSignature& root,
		const Config& config, InputLayout::Spec ilSpec = InputLayout::Spec::serial
	);

	RenderProtocol makeProtocol( D3D12Device& device, const RenderProtocol::Desc& desc) {
		return RenderProtocol( device, *this,
			selectBlobsStrong<ShaderBlob::Type::Vertex>(), desc
		);
	}

	std::size_t maxInstanceCnt() const noexcept {
		return maxInstanceCnt_;
	}

	std::size_t maxDrawcallCnt() const noexcept {
		return maxDrawcallCnt_;
	}

	void bindRootParams(D3D12GfxCmdList& cmdList) override;
	void bindPerDrawcallData(std::size_t drawcallIdx, D3D12GfxCmdList& cmdList);

	void loadBlobs() override;
	void releaseBlobs() override;

	UploadBuffer perFrameData_;
	UploadBuffer perDrawcallData_;
	UploadBuffer perInstanceData_;

	std::size_t cbDrawcallDataSize() const noexcept {
		return cbDrawcallDataSize_;
	}

private:
	static InputLayout makeInputLayout(InputLayout::Spec ilSpec);
	static InputLayout makeInputLayoutSerial();
	static InputLayout makeInputLayoutSeparated();

	std::size_t maxInstanceCnt_;
	std::size_t maxDrawcallCnt_;
};

class ShaderCascadeShadowMap : public Shader {
private:
	std::size_t cbDrawcallDataSize_;

public:
	struct Config {
		std::size_t maxInstanceCnt;
		std::size_t maxDrawcallCnt;
	};

	ShaderCascadeShadowMap(D3D12Device& device, const RootSignature& root,
		const Config& config, InputLayout::Spec ilSpec = InputLayout::Spec::serial
	);

	RenderProtocol makeProtocol(D3D12Device& device, const RenderProtocol::Desc& desc) {
		return RenderProtocol(device, *this,
			selectBlobsStrong<ShaderBlob::Type::Vertex>(), desc
		);
	}

	std::size_t maxInstanceCnt() const noexcept {
		return maxInstanceCnt_;
	}

	std::size_t maxDrawcallCnt() const noexcept {
		return maxDrawcallCnt_;
	}

	void bindRootParams(D3D12GfxCmdList& cmdList) override;
	void bindPerDrawcallData(std::size_t drawcallIdx, D3D12GfxCmdList& cmdList);

	void loadBlobs() override;
	void releaseBlobs() override;

	UploadBuffer perFrameData_[3];
	UploadBuffer perDrawcallData_;
	UploadBuffer perInstanceData_;
	int curCascadeIdx_;

	std::size_t cbDrawcallDataSize() const noexcept {
		return cbDrawcallDataSize_;
	}

private:
	static InputLayout makeInputLayout(InputLayout::Spec ilSpec);
	static InputLayout makeInputLayoutSerial();
	static InputLayout makeInputLayoutSeparated();

	std::size_t maxInstanceCnt_;
	std::size_t maxDrawcallCnt_;
};

class ShaderShadowMapAnimated : public Shader {
private:
	std::size_t cbDrawcallDataSize_;

public:
	struct Config {
		std::size_t maxInstanceCnt;
		std::size_t maxDrawcallCnt;
		std::size_t maxBoneCnt;
	};

	ShaderShadowMapAnimated( D3D12Device& device, const RootSignature& root,
		const Config& config, InputLayout::Spec ilSpec = InputLayout::Spec::serial
	);

	RenderProtocol makeProtocol( D3D12Device& device, const RenderProtocol::Desc& desc) {
		return RenderProtocol( device, *this,
			selectBlobsStrong<ShaderBlob::Type::Vertex>(), desc
		);
	}

	std::size_t maxInstanceCnt() const noexcept {
		return maxInstanceCnt_;
	}

	std::size_t maxDrawcallCnt() const noexcept {
		return maxDrawcallCnt_;
	}

	std::size_t maxBoneCnt() const noexcept {
		return maxBoneCnt_;
	}

	void bindRootParams(D3D12GfxCmdList& cmdList) override;
	void bindPerDrawcallData(std::size_t drawcallIdx, D3D12GfxCmdList& cmdList);

	void loadBlobs() override;
	void releaseBlobs() override;

	UploadBuffer perFrameData_;
	UploadBuffer perDrawcallData_;
	UploadBuffer perInstanceData_;

	std::size_t cbDrawcallDataSize() const noexcept {
		return cbDrawcallDataSize_;
	}

private:
	static InputLayout makeInputLayout(InputLayout::Spec ilSpec);
	static InputLayout makeInputLayoutSerial();
	static InputLayout makeInputLayoutSeparated();

	std::size_t maxInstanceCnt_;
	std::size_t maxDrawcallCnt_;
	std::size_t maxBoneCnt_;
};

class ShaderCascadeShadowMapAnimated : public Shader {
private:
	std::size_t cbDrawcallDataSize_;

public:
	struct Config {
		std::size_t maxInstanceCnt;
		std::size_t maxDrawcallCnt;
		std::size_t maxBoneCnt;
	};

	ShaderCascadeShadowMapAnimated( D3D12Device& device, const RootSignature& root,
		const Config& config, InputLayout::Spec ilSpec = InputLayout::Spec::serial
	);

	RenderProtocol makeProtocol( D3D12Device& device, const RenderProtocol::Desc& desc) {
		return RenderProtocol( device, *this,
			selectBlobsStrong<ShaderBlob::Type::Vertex>(), desc
		);
	}

	std::size_t maxInstanceCnt() const noexcept {
		return maxInstanceCnt_;
	}

	std::size_t maxDrawcallCnt() const noexcept {
		return maxDrawcallCnt_;
	}

	std::size_t maxBoneCnt() const noexcept {
		return maxBoneCnt_;
	}

	void bindRootParams(D3D12GfxCmdList& cmdList) override;
	void bindPerDrawcallData(std::size_t drawcallIdx, D3D12GfxCmdList& cmdList);

	void loadBlobs() override;
	void releaseBlobs() override;

	UploadBuffer perFrameData_[3];
	UploadBuffer perDrawcallData_;
	UploadBuffer perInstanceData_;
	int curCascadeIdx_;

	std::size_t cbDrawcallDataSize() const noexcept {
		return cbDrawcallDataSize_;
	}

private:
	static InputLayout makeInputLayout(InputLayout::Spec ilSpec);
	static InputLayout makeInputLayoutSerial();
	static InputLayout makeInputLayoutSeparated();

	std::size_t maxInstanceCnt_;
	std::size_t maxDrawcallCnt_;
	std::size_t maxBoneCnt_;
};

class ShaderScreenQuad : public Shader {
public:
	ShaderScreenQuad(D3D12Device& device, const RootSignature& root);

	RenderProtocol makeProtocol( D3D12Device& device, const RenderProtocol::Desc& desc) {
		return RenderProtocol( device, *this,
			selectBlobsStrong<ShaderBlob::Type::Vertex, ShaderBlob::Type::Pixel>(), desc
		);
	}

	void bindRootParams(D3D12GfxCmdList& cmdList) override;

	void loadBlobs() override;
	void releaseBlobs() override;

	void draw(D3D12GfxCmdList& cmdList) const {
		screenQuad_.draw(cmdList);
	}

	void setScreenTexture(Texture* pScreenTexture) {
		screenQuad_.link(pScreenTexture);
	}

	UploadBuffer perDrawcallData_;
	ScreenQuad screenQuad_;
};

class ShaderTessellation : public Shader {
private:
	std::size_t cbDrawcallDataSize_;

public:
	struct Config {
		std::size_t maxInstanceCnt;
		std::size_t maxDrawcallCnt;
		std::size_t maxLightCnt;
	};

	ShaderTessellation( D3D12Device& device, const RootSignature& root, const Config& config,
		InputLayout::Spec ilSpec = InputLayout::Spec::serial
	);

	RenderProtocol makeProtocol( D3D12Device& device, const RenderProtocol::Desc& desc) {
		return RenderProtocol( device, *this,
			selectBlobsStrong<ShaderBlob::Type::Vertex, ShaderBlob::Type::Hull, ShaderBlob::Type::Domain, ShaderBlob::Type::Pixel>(), desc
		);
	}	

	void bindRootParams(D3D12GfxCmdList& cmdList) override;
	void bindPerDrawcallData(std::size_t drawcallIdx, D3D12GfxCmdList& cmdList);

	std::size_t cbDrawcallDataSize() const noexcept {
		return cbDrawcallDataSize_;
	}

	void loadBlobs() override;
	void releaseBlobs() override;

	std::size_t maxInstanceCnt() const noexcept {
		return maxInstanceCnt_;
	}

	std::size_t maxDrawcallCnt() const noexcept {
		return maxDrawcallCnt_;
	}

	std::size_t maxLightCnt() const noexcept {
		return maxLightCnt_;
	}

	void draw(D3D12GfxCmdList& cmdList, const LevelChunkModel& chunk) const {
		chunk.draw(cmdList);
	}

	UploadBuffer perInstanceData_;
	UploadBuffer perDrawcallData_;
	UploadBuffer perFrameData_;
	UploadBuffer lightBuffer_;

private:
	static InputLayout makeInputLayout(InputLayout::Spec ilSpec);
	static InputLayout makeInputLayoutSerial();
	static InputLayout makeInputLayoutSeparated();

	std::size_t maxInstanceCnt_;
	std::size_t maxDrawcallCnt_;
	std::size_t maxLightCnt_;
};

class ShaderShadowMapTessellation : public Shader {
private:
	std::size_t cbDrawcallDataSize_;

public:
	struct Config {
		std::size_t maxInstanceCnt;
		std::size_t maxDrawcallCnt;
	};

	ShaderShadowMapTessellation( D3D12Device& device, const RootSignature& root,
		const Config& config, InputLayout::Spec ilSpec = InputLayout::Spec::serial
	);

	RenderProtocol makeProtocol( D3D12Device& device, const RenderProtocol::Desc& desc) {
		return RenderProtocol( device, *this,
			selectBlobsStrong<ShaderBlob::Type::Vertex, ShaderBlob::Type::Hull, ShaderBlob::Type::Domain>(), desc
		);
	}

	std::size_t maxInstanceCnt() const noexcept {
		return maxInstanceCnt_;
	}

	std::size_t maxDrawcallCnt() const noexcept {
		return maxDrawcallCnt_;
	}

	void bindRootParams(D3D12GfxCmdList& cmdList) override;
	void bindPerDrawcallData(std::size_t drawcallIdx, D3D12GfxCmdList& cmdList);

	void loadBlobs() override;
	void releaseBlobs() override;

	void draw(D3D12GfxCmdList& cmdList, const LevelChunkModel& chunk) const {
		chunk.draw(cmdList);
	}

	UploadBuffer perFrameData_[3];
	UploadBuffer perDrawcallData_;
	UploadBuffer perInstanceData_;
	int curCascadeIdx_;

	std::size_t cbDrawcallDataSize() const noexcept {
		return cbDrawcallDataSize_;
	}

private:
	static InputLayout makeInputLayout(InputLayout::Spec ilSpec);
	static InputLayout makeInputLayoutSerial();
	static InputLayout makeInputLayoutSeparated();

	std::size_t maxInstanceCnt_;
	std::size_t maxDrawcallCnt_;
};

class ShaderMatMul : public ComputeShader {
public:
	static constexpr std::size_t groupSizeX = 256u;

	struct Config {
		std::size_t maxMatrixCnt;
	};

	ShaderMatMul(D3D12Device& device, const RootSignature& root, const Config& config);

	ComputeProtocol makeProtocol(D3D12Device& device, const ComputeProtocol::Desc& desc) {
		loadBlobIfNot();
		return ComputeProtocol(device, *this, blob_.value(), desc);
	}	

	void bindRootParams(D3D12GfxCmdList& cmdList) override;
	void loadBlob() override;
	void releaseBlob() override;

	std::size_t maxMatrixCnt() const noexcept {
		return maxMatrixCnt_;
	}

	void dispatch(D3D12GfxCmdList& cmdList, std::size_t threadGroupCnt) {
		cmdList.get()->Dispatch(
			static_cast<UINT>(threadGroupCnt), 1u, 1u
		);
	}

	UploadBuffer lhsMatrices_;
	UploadBuffer rhsMatrices_;
	ReadbackBuffer resultMatrices_;
	DefaultBuffer resultMatricesSrc_;
	
private:
	std::size_t maxMatrixCnt_;
};

struct KeyFrame {
    mu::Vec3 pos;
    mu::NQuat rot;
    mu::Vec3 scale;
    float time;
};

class ShaderAnimInterpolation : public ComputeShader {
public:
	static constexpr std::size_t groupSizeX = 256u;

	struct Config {
		std::size_t maxKeyFrameCnt;
	};

	ShaderAnimInterpolation(D3D12Device& device, const RootSignature& root, const Config& config);

	ComputeProtocol makeProtocol(D3D12Device& device, const ComputeProtocol::Desc& desc) {
		loadBlobIfNot();
		return ComputeProtocol(device, *this, blob_.value(), desc);
	}	

	void bindRootParams(D3D12GfxCmdList& cmdList) override;
	void loadBlob() override;
	void releaseBlob() override;

	std::size_t maxKeyFrameCnt() const noexcept {
		return maxKeyFrameCnt_;
	}

	void dispatch(D3D12GfxCmdList& cmdList, std::size_t threadGroupCnt) {
		cmdList.get()->Dispatch(
			static_cast<UINT>(threadGroupCnt), 1u, 1u
		);
	}

	UploadBuffer lhsKeyFrames_;
	UploadBuffer rhsKeyFrames_;
	ReadbackBuffer resultMatrices_;
	DefaultBuffer resultMatricesSrc_;

private:
	std::size_t maxKeyFrameCnt_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __D3D12ShaderXX_HPP