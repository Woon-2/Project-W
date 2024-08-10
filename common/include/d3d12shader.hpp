#ifndef __SHADER_HPP
#define __SHADER_HPP

#include "d3d12InputLayout.hpp"

#include <d3d12.h>

#include <d3dcompiler.h>
#include "dxtarget.hpp"
#include "dxexcept.hpp"

#include <map>
#include <filesystem>


namespace gfx {

namespace d3d12 {

class Shader {
public:
    friend class ShaderBuilder;

    enum class Type {
        Vertex,
        Pixel
    };

    struct Desc {
        wrl::ComPtr<ID3D12RootSignature> pRootSignature;
        D3D12_STREAM_OUTPUT_DESC streamOutput;
        D3D12_BLEND_DESC blend;
        UINT sampleMask;
        D3D12_RASTERIZER_DESC rasterizerState;
        D3D12_DEPTH_STENCIL_DESC depthStencilState;
        InputLayout inputLayout;
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

    using Idx = int;

    static wrl::ComPtr<ID3DBlob> loadCSO(const std::wstring& path) {
        wrl::ComPtr<ID3DBlob> blob;
        DX_THROW_FAILED( D3DReadFileToBlob(path.c_str(), &blob) );
        return blob;
    }

    static wrl::ComPtr<ID3DBlob> loadCSO(const std::filesystem::path& path) {
        return loadCSO(path.wstring());
    }

    void code(Type type, wrl::ComPtr<ID3DBlob> content) {
        codes_[type] = std::move(content);
    }
    void make(ID3D12Device* pDevice, Idx idx, const Desc& desc);
    void bind(ID3D12GraphicsCommandList* pCmdList, Idx idx) const;

private:
    std::map< Type, wrl::ComPtr<ID3DBlob> > codes_;
    std::map< Idx, wrl::ComPtr<ID3D12PipelineState> > psos_;
    wrl::ComPtr<ID3D12RootSignature> pRoot_;
    InputLayout inputLayout_;
};

class ShaderBuilder {
protected:
    Shader::Desc desc_;

public:
    ShaderBuilder() NOEXCEPT = default;
    ShaderBuilder(const Shader::Desc& desc) NOEXCEPT : desc_(desc) {}

    ShaderBuilder& code(Shader::Type type, wrl::ComPtr<ID3DBlob> content) {
        codes_[type] = std::move(content);
        return *this;
    }

    ShaderBuilder& root(wrl::ComPtr<ID3D12RootSignature> pRootSignature) NOEXCEPT {
        desc_.pRootSignature = pRootSignature;
        return *this;
    }

    ShaderBuilder& streamOutput(const D3D12_STREAM_OUTPUT_DESC& desc) NOEXCEPT {
        desc_.streamOutput = desc;
        return *this;
    }

    ShaderBuilder& blend(const D3D12_BLEND_DESC& desc) NOEXCEPT {
        desc_.blend = desc;
        return *this;
    }

    ShaderBuilder& sampleMask(UINT mask) NOEXCEPT {
        desc_.sampleMask = mask;
        return *this;
    }

    ShaderBuilder& rasterizer(const D3D12_RASTERIZER_DESC& desc) NOEXCEPT {
        desc_.rasterizerState = desc;
        return *this;
    }

    ShaderBuilder& depthStencil(const D3D12_DEPTH_STENCIL_DESC& desc) NOEXCEPT {
        desc_.depthStencilState = desc;
        return *this;
    }

    ShaderBuilder& inputLayout(const InputLayout& il) {
        desc_.inputLayout = il;
        return *this;
    }

    ShaderBuilder& inputLayout(InputLayout&& il) {
        desc_.inputLayout = std::move(il);
        return *this;
    }
    
    ShaderBuilder& indexBufferStripCut(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE value) NOEXCEPT {
        desc_.ibStripCutValue = value;
        return *this;
    }

    ShaderBuilder& primitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type) NOEXCEPT {
        desc_.primitiveTopologyType = type;
        return *this;
    }

    ShaderBuilder& numRenderTargets(UINT num) NOEXCEPT {
        desc_.numRenderTargets = num;
        return *this;
    }

    template <std::ranges::range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, DXGI_FORMAT>
            && std::ranges::sized_range<R>
    ShaderBuilder& rtvFormats(const R& formats) {
        if (formats.size() > 8) {
            throw std::invalid_argument("The number of render target formats must be less than or equal to 8.");
        }

        for (auto format : formats) {
            desc_.rtvFormats[desc_.numRenderTargets++] = format;
        }
        return *this;
    }

    ShaderBuilder& dsvFormat(DXGI_FORMAT format) NOEXCEPT {
        desc_.dsvFormat = format;
        return *this;
    }

    ShaderBuilder& sampleDesc(const DXGI_SAMPLE_DESC& desc) NOEXCEPT {
        desc_.sampleDesc = desc;
        return *this;
    }

    ShaderBuilder& nodeMask(UINT mask) NOEXCEPT {
        desc_.nodeMask = mask;
        return *this;
    }

    ShaderBuilder& cachedPso(const D3D12_CACHED_PIPELINE_STATE& pso) NOEXCEPT {
        desc_.cachedPSO = pso;
        return *this;
    }

    ShaderBuilder& flags(D3D12_PIPELINE_STATE_FLAGS flags) NOEXCEPT {
        desc_.flags = flags;
        return *this;
    }

    void build(ID3D12Device* pDevice, Shader& shader, Shader::Idx idx) {
        shader.codes_ = codes_;
        shader.make(pDevice, idx, desc_);
    }

private:
    std::map< Shader::Type, wrl::ComPtr<ID3DBlob> > codes_;
};

class SimpleShaderBuilder : private ShaderBuilder {
public:
    SimpleShaderBuilder() NOEXCEPT
        : ShaderBuilder( Shader::Desc{
            .blend = blendDesc(),
            .sampleMask = UINT_MAX,
            .rasterizerState = rasterizerDesc(),
            .depthStencilState = depthStencilDesc(),
            .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .numRenderTargets = 1u,
            .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
            .dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT,
            .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u }
        } ) {}

    SimpleShaderBuilder& code(Shader::Type type, wrl::ComPtr<ID3DBlob> content) {
        ShaderBuilder::code(type, std::move(content));
        return *this;
    }

    SimpleShaderBuilder& wireframe() NOEXCEPT {
        desc_.rasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
        return *this;
    }

    SimpleShaderBuilder& solid() NOEXCEPT {
        desc_.rasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        return *this;
    }

    SimpleShaderBuilder& cullFront() NOEXCEPT {
        desc_.rasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
        return *this;
    }

    SimpleShaderBuilder& cullBack() NOEXCEPT {
        desc_.rasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        return *this;
    }

    SimpleShaderBuilder& cullNone() NOEXCEPT {
        desc_.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        return *this;
    }

    SimpleShaderBuilder& depthEnable() NOEXCEPT {
        desc_.depthStencilState.DepthEnable = true;
        return *this;
    }

    SimpleShaderBuilder& depthDisable() NOEXCEPT {
        desc_.depthStencilState.DepthEnable = false;
        return *this;
    }

    SimpleShaderBuilder& setRoot(wrl::ComPtr<ID3D12RootSignature> pRootSignature) NOEXCEPT {
        ShaderBuilder::root(pRootSignature);
        return *this;
    }

    SimpleShaderBuilder& setInputLayout(const InputLayout& il) {
        ShaderBuilder::inputLayout(il);
        return *this;
    }

    SimpleShaderBuilder& setInputLayout(InputLayout&& il) {
        ShaderBuilder::inputLayout(std::move(il));
        return *this;
    }

    void build(ID3D12Device* pDevice, Shader& shader, Shader::Idx idx) {
        ShaderBuilder::build(pDevice, shader, idx);
    }

private:
    D3D12_BLEND_DESC blendDesc() const NOEXCEPT {
        return D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .LogicOpEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .LogicOp = D3D12_LOGIC_OP_NOOP,
                    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
                }
            }
        };
    }

    D3D12_RASTERIZER_DESC rasterizerDesc() const NOEXCEPT {
        return D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_BACK,
            .FrontCounterClockwise = false,
            .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
            .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
            .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
            .DepthClipEnable = true,
            .MultisampleEnable = false,
            .AntialiasedLineEnable = false,
            .ForcedSampleCount = 0,
            .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
        };
    }

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc() const NOEXCEPT {
        return D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = false,
            .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
            .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
            .FrontFace = D3D12_DEPTH_STENCILOP_DESC{
                .StencilFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilPassOp = D3D12_STENCIL_OP_KEEP,
                .StencilFunc = D3D12_COMPARISON_FUNC_NEVER
            },
            .BackFace = D3D12_DEPTH_STENCILOP_DESC{
                .StencilFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilPassOp = D3D12_STENCIL_OP_KEEP,
                .StencilFunc = D3D12_COMPARISON_FUNC_NEVER
            }
        };
    }
};

} // namespace d3d12

} // namespace gfx

#endif  // __SHADER_HPP