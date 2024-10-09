#ifndef __SHADER_HPP
#define __SHADER_HPP

#include "shader.hpp"

#include "d3d12Descriptor.hpp"
#include "d3d12InputLayout.hpp"

#include "directx/d3dx12.h"
#include "directx/d3d12.h"

#include <d3dcompiler.h>
#include "dxtarget.hpp"
#include "dxexcept.hpp"

#include <map>
#include <filesystem>

#include "config.hpp"


namespace gfx {

namespace d3d12 {

/**
 * @brief A class representing a shader in D3D12.    
 * A Shader determines how the objects are rendered and given information is interpreted in the pipeline.     
 * It stores several settings with user-provided indices, and can be bound as specific setting to the pipeline by the index.
 * @details The shader codes are obtained from compiled shader object files (CSO) with the extension ".cso".    
 * The static member function Shader::loadCSO is provided for loading CSO files.
 * 
 * The read codes are stored in the Shader instance via Shader::code,    
 * and with a descriptor which can be considered as the setting of the shader including root signature, InputLayout, etc.,    
 * The shader is created at an index through Shader::make.    
 * 
 * A Shader can be easily bound to the pipeline by calling Shader::bind with the index.     
 * 
 * Internally, it stores the compiled shader object files for each shader type,    
 * and pipeline state objects (PSOs) for each index.
 * @see Shader::Desc InputLayout ShaderBuilder SimpleShaderBuilder
 */
class Shader : public gfx::Shader<InputLayout> {
protected:
    void defPreDraw( IRenderContext& ctx, const IScene& scene,
        IRenderTarget& target, rp::Protocol protocol
    );

public:
    friend class ShaderBuilder;

    /**
     * @brief Type of the shader.
     * @details The shader can be either a vertex shader or a pixel shader currently.
     */
    enum class Type {
        Vertex,
        Pixel
    };

    template <class T>
    using Cont = std::vector<T>;

    /**
     * @brief Descriptor for the shader.
     * @details The descriptor contains all the settings for the shader,    
     * including root signature, stream output, blend, rasterizer, depth stencil, input layout, etc.    
     * @note As the shader requires a root signature, the root signature to use must be created before creating the shader.
     * @see InputLayout ShaderBuilder SimpleShaderBuilder
     */
    struct Desc {
        wrl::ComPtr<ID3D12RootSignature> pRootSignature;
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

    using Idx = int;

    struct BindOption {
        Idx idx;
        ILIdx ilIdx;
    };

    Shader() NOEXCEPT = default;
    virtual ~Shader() = default;
    Shader(const Shader&) = default;
    Shader(Shader&&) noexcept = default;
    Shader& operator=(const Shader&) = default;
    Shader& operator=(Shader&&) noexcept = default;

    /**
     * @brief Loads a compiled shader object file (CSO) from filesystem.    
     * @param path The path to the CSO file.
     * @return wrl::ComPtr<ID3DBlob> The loaded CSO.
     * @throw DXException if the loading fails.
     * @see code
     */
    static wrl::ComPtr<ID3DBlob> loadCSO(const std::wstring& path) {
        wrl::ComPtr<ID3DBlob> blob;
        DX_THROW_FAILED( D3DReadFileToBlob(path.c_str(), &blob) );
        return blob;
    }
    /**
     * @brief Loads a compiled shader object file (CSO) from filesystem.    
     * @param path The path to the CSO file.
     * @return wrl::ComPtr<ID3DBlob> The loaded CSO.
     * @throw DXException if the loading fails.
     * @see code
     */
    static wrl::ComPtr<ID3DBlob> loadCSO(const std::filesystem::path& path) {
        return loadCSO(path.wstring());
    }
    /**
     * @brief Stores a compiled shader object as the byte code for the given type of shader.
     * @param type The type of the shader.
     * @param content The byte code of the shader.
     * @see loadCSO
     */
    void code(Type type, wrl::ComPtr<ID3DBlob> content) {
        codes_[type] = std::move(content);
    }
    /**
     * @brief Creates a shader at the given index with descriptor.
     * @param pDevice The device to create the shader.
     * @param idx The index of the shader.
     * @param desc The descriptor for the shader.
     * @throw DXException if the creation fails.
     * @see bind
     */
    void make(ID3D12Device* pDevice, Idx idx, const Desc& desc);
    void bind(IRenderContext& ctx, std::any option) const override;

private:
    std::map< Type, wrl::ComPtr<ID3DBlob> > codes_;
    std::map< Idx, Cont<wrl::ComPtr<ID3D12PipelineState>> > psosMap_;
    wrl::ComPtr<ID3D12RootSignature> pRoot_;
};

/**
 * @brief A builder class for creating a shader with a descriptor.    
 * It features as staging the settings to create a shader and reusing the settings for multiple shaders with little modification. 
 * @note If your shaders has some pattern with the settings,    
 * derive a class from ShaderBuilder and provide handy preset functions for the settings.
 * @see Shader Shader::Desc SimpleShaderBuilder
 */
class ShaderBuilder {
protected:
    /**
     * @brief Descriptor for the shader.    
     * It can be directly modified by the derived class.
     */
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

/**
 * @brief A ShaderBuilder with simple presets that is commonly used.
 * @details Back face culling, solid fill mode, and depth test are enabled by default with other default settings.
 * @note It isn't sufficient to build a shader immediately from default constructed SimpleShaderBuilder.    
 * The shader code, input layout, and root signature must be set before building the shader.
 * @see ShaderBuilder Shader Shader::Desc
 */
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
            .dsvFormat = DXGI_FORMAT_D32_FLOAT,
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