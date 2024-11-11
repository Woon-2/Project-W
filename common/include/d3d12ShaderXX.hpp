#ifndef __D3D12ShaderXX_HPP
#define __D3D12ShaderXX_HPP

#include "d3d12Low.hpp"
#include "d3d12ResourceXX.hpp"

#include "gfxExcept.hpp"

#include <bitset>
#include <vector>

#include "enumUtil.hpp"

namespace gfx {

namespace d3d12 {

class InputLayout {
public:
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
		for (std::size_t i = 0; i < slots_.size(); ++i) {
			const auto& slot = slots_[i];
			if (slot.attributes != vbs[i].attributes()) {
				return false;
			}
		}
		return true;
	}

private:
	std::vector<Slot> slots_;
};

class ShaderBlob : public D3DWrapper<ID3DBlob> {
public:
	enum class Type {
		Vertex,
		Pixel,
		Geometry,
		Size
	};

	ShaderBlob( const std::filesystem::path& path,
		const InputLayout& inputLayout, const D3D_SHADER_MACRO* macros,
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

class RenderProtocol : public D3DWrapper<ID3D12PipelineState> {
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

	bool compatibleWith(const RefMesh& mesh) const;

	Shader& shader() const noexcept { return *pShader_; }

private:
	Shader* pShader_;
};

class Shader {
public:
	Shader(const RootSignature& root)
		: blobs_(etoi(ShaderBlob::Type::Size)), inputLayout_(), root_(root) {}

	Shader(const RootSignature& root, const InputLayout& il)
		: blobs_(etoi(ShaderBlob::Type::Size)),
		inputLayout_(il), root_(root) {}

	Shader(const RootSignature& root, InputLayout&& il)
		: blobs_(etoi(ShaderBlob::Type::Size)),
		inputLayout_(std::move(il)), root_(root) {}

	const auto& blobs() const noexcept {
		return blobs_;
	}

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

	void setInputLayout(const InputLayout& inputLayout) {
		inputLayout_ = inputLayout;
	}

	const InputLayout& inputLayout() const noexcept {
		return inputLayout_;
	}

	const RootSignature& rootSiganture() const noexcept {
		return root_;
	}

	void draw(D3D12GfxCmdList& cmdList, const Mesh& mesh, std::size_t instanceCnt) {
		mesh.draw(cmdList, instanceCnt);
	}

protected:
	std::vector<std::optional<ShaderBlob>> blobs_;
	InputLayout inputLayout_;
	RootSignature root_;
};

inline bool RenderProtocol::compatibleWith(const RefMesh& mesh) const {
    pShader_->inputLayout().checkBindable(mesh.vbs());
}

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __D3D12ShaderXX_HPP