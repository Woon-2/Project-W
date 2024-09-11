#ifndef __D3D12CORE_HPP
#define __D3D12CORE_HPP

#include "gfx.hpp"

#include "d3d12shader.hpp"
#include "d3d12InputLayout.hpp"

#include <directx/d3dx12.h>
#include <directx/d3d12.h>

#include "dxfactory.hpp"
#include "dxtarget.hpp"
#include "dxexcept.hpp"

#include <memory>
#include <vector>
#include <array>
#include <map>
#include <string>
#include <list>

#define ENABLE_D3D12_WINDOW

#ifdef ENABLE_D3D12_WINDOW
#include "d3dwindow.hpp"
#endif  // ENABLE_D3D12_WINDOW

#include "config.hpp"

/**
 * @file d3d12core.hpp
 */

namespace gfx {

namespace wrl = Microsoft::WRL;

using namespace std::literals;

/**
 * @brief The namespace for the D3D12 implementation of the GFX library.
 * @see gfx
 */
namespace d3d12 {

class CmdListPool {
public:
    struct Element {
        wrl::ComPtr<ID3D12GraphicsCommandList> pCmdList;
        wrl::ComPtr<ID3D12CommandAllocator> pCmdAlloc;
    };

    CmdListPool() = default;
    CmdListPool(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type, std::size_t size);
    const Element fetch();

    void push(Element&& elem) {
        cmdLists_.push_back(std::move(elem.pCmdList));
        cmdAllocs_.push_back(std::move(elem.pCmdAlloc));
    }

    void push(const Element& elem) {
        cmdLists_.push_back(elem.pCmdList);
        cmdAllocs_.push_back(elem.pCmdAlloc);
    }

    void clear() {
        cmdLists_.clear();
        cmdAllocs_.clear();
    }

private:
    std::list<wrl::ComPtr<ID3D12GraphicsCommandList>> cmdLists_;
    std::list<wrl::ComPtr<ID3D12CommandAllocator>> cmdAllocs_;
};

class DescriptorHeap {
public:
    DescriptorHeap()
        : pHeap_(), cpuStart_(), gpuStart_(), type_(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES), size_(0), capacity_(0), stride_(0), shaderVisible_(false) {}
    DescriptorHeap(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, std::size_t capacity, bool shaderVisible = false);

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle(std::size_t idx = 0) const;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle(std::size_t idx = 0) const;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandleUninit(std::size_t idx = 0) const;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandleUninit(std::size_t idx = 0) const;

    void pushSrv(ID3D12Device* pDevice, ID3D12Resource* pRes, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc);
    void pushSrv(ID3D12Device* pDevice, ID3D12Resource* pRes);
    void pushRtv(ID3D12Device* pDevice, ID3D12Resource* pRes, const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc);
    void pushRtv(ID3D12Device* pDevice, ID3D12Resource* pRes);
    void pushDsv(ID3D12Device* pDevice, ID3D12Resource* pRes, const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc);
    void pushDsv(ID3D12Device* pDevice, ID3D12Resource* pRes);
    void pushUav(ID3D12Device* pDevice, ID3D12Resource* pRes, const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc);
    void pushCbv(ID3D12Device* pDevice, const D3D12_CONSTANT_BUFFER_VIEW_DESC& cbvDesc);
    void pushSam(ID3D12Device* pDevice, const D3D12_SAMPLER_DESC& samplerDesc);

    void set(ID3D12GraphicsCommandList* pCmdList) {
        DX_THROW_FAILED_VOID(pCmdList->SetDescriptorHeaps(1u, pHeap_.GetAddressOf()));
    }

    std::size_t size() const NOEXCEPT {
        return size_;
    }

    std::size_t stride() const NOEXCEPT {
        return stride_;
    }

    D3D12_DESCRIPTOR_HEAP_TYPE type() const NOEXCEPT {
        return type_;
    }

private:
    wrl::ComPtr<ID3D12DescriptorHeap> pHeap_;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart_;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart_;
    std::size_t size_;
    std::size_t capacity_;
    std::size_t stride_;
    D3D12_DESCRIPTOR_HEAP_TYPE type_;
    bool shaderVisible_;
};

/**
 * @brief The core class for the D3D12 implementation of the GFX library.     
 * It is responsible for initializing the device, the command list & queue, and the descriptor heaps,    
 * and for managing the root signatures, the shaders, the input layouts, and the temporary upload buffers.    
 * 
 * It has 4 maps each for root signatures, shaders, input layouts, and temporary upload buffers.     
 * It is recommended to register the root signatures, the shaders, the input layouts, and the temporary upload buffers to Core,    
 * and acquire them from Core when needed.    
 * 
 * Core::preRender does resetting the command list, and Core::postRender does closing the command list and executing it.    
 * As Core::preRender and Core::postRender doesn't handle gpu-cpu synchronization,     
 * it supports Core::waitForGpu and Core::alterFence for that purpose.    
 * So, when you work with d3d12, ICore interface isn't sufficient at all, and you need to downcast it to the Core for gpu-cpu synchronization.     
 * 
 * Before initializing the Core, it is required to configure the DXGI Factory, the descriptor heaps' sizes.    
 * They're considered as initialization parameters, and calling `config ~` static member functions considered as passing the arguments.
 * 
 * @note As Core manages the root signatures, the shaders, the input layouts, and the temporary upload buffers using std::map,     
 * the key of them must be unique.
 * 
 * @see ICore
 */
class Core : public ICore {
private:
    class RootNotFound : public gfx::Exception {
    public:
        RootNotFound(int lineNum, const char* fileStr, std::string_view idx)
            : gfx::Exception(lineNum, fileStr,
                "The root signature with index \""s + idx.data() + "\" does not exist.\n"s
                + "Please check if the root signature is registered to the Core.\n"s
                + "or the root signature is already popped."s
            ) {}
    };

    class UpBufIdxNotFound : public gfx::Exception {
    public:
        UpBufIdxNotFound(int lineNum, const char* fileStr, std::string_view idx)
            : gfx::Exception(lineNum, fileStr,
                "The temporary upload buffer with index \""s + idx.data() + "\" does not exist.\n"s
                + "Please check if the temporary upload buffer is registered to the Core\n"s
                + "or the temporary upload buffer is already popped."s
            ) {}
    };

    class ShaderIdxNotFound : public gfx::Exception {
    public:
        ShaderIdxNotFound(int lineNum, const char* fileStr, std::string_view idx)
            : gfx::Exception(lineNum, fileStr,
                "The shader with index \""s + idx.data() + "\" does not exist.\n"s
                + "Please check if the shader is registered to the Core.\n"s
                + "or the shader is already popped."s
            ) {}
    };

    class InputLayoutIdxNotFound : public gfx::Exception {
    public:
        InputLayoutIdxNotFound(int lineNum, const char* fileStr, std::string_view idx)
            : gfx::Exception(lineNum, fileStr,
                "The input layout with index \""s + idx.data() + "\" does not exist.\n"s
                + "Please check if the input layout is registered to the Core.\n"s
                + "or the input layout is already popped."s
            ) {}
    };

public:
    friend class D3D12RenderContext;
#ifdef ENABLE_D3D12_WINDOW
    friend class WindowAttorney;
#endif  // ENABLE_D3D12_WINDOW
    friend class DeviceFetcher;

    using RootIdx = std::string;
    using UpBufIdx = std::string;
    using ShaderIdx = std::string;
    using InputLayoutIdx = std::string;
    using DescHeapIdx = std::string;

    /**
     * @brief Configures the DXGI Factory.
     * @param factory The DXGI Factory.
     * @note The factory can be acquired through DXFactory.
     * @see DXFactory Core::init
     */
    static void configDXFactory(wrl::ComPtr<IDXGIFactory4> factory) NOEXCEPT {
        spFactory = factory;
    }
    static wrl::ComPtr<IDXGIFactory4> dxFactory() NOEXCEPT {
        return spFactory;
    }

    static void configCmdListPoolSize(std::size_t size) NOEXCEPT {
        sCmdListPoolSize = size;
    }
    static std::size_t cmdListPoolSize() NOEXCEPT {
        return sCmdListPoolSize;
    }

    static void configFenceCnt(std::size_t cnt) NOEXCEPT {
        sFenceCnt = cnt;
    }
    static std::size_t fenceCnt() NOEXCEPT {
        return sFenceCnt;
    }

    /**
     * @brief Initializes the Core.    
     * It creates the device, the command queue & list, the descriptor heaps, and the fence & event.
     * @throws DXException When the core creation fails.
     */
    void init() override;
    /**
     * @brief Renders the `scene` using the `renderer` to the `target`. 
     * @param scene The scene to render.
     * @param renderer The renderer to use.
     * @param target The target to render to.
     * @see IScene IRenderer IRenderTarget
     */
    void render(IRenderContext& ctx, const IScene& scene, const IRenderer& renderer, IRenderTarget& target) override;
    /**
     * @brief Prepares for rendering.    
     * It resets the command list.
     * @throws DXException When the command list reset fails.
     */
    void preRender() override;
    /**
     * @brief Finalizes the rendering.    
     * It closes the command list and executes it.
     * @throws DXException When the command list close or execute fails.
     */
    void postRender() override;
    void cleanup() override;
     
    /**
     * @brief Creates a render context for D3D12.
     * @return `std::unique_ptr<IRenderContext>` The created render context.
     * @see IRenderContext D3D12RenderContext
     */
    std::unique_ptr<IRenderContext> createContext() override;

    /**
     * @brief Waits for the GPU to finish the work.    
     * The caller thread will be blocked until the GPU finishes the work.
     * @details It waits for the reaching of the current fence.
     * @throws DXException When the fence signal or event on completion fails.
     */
    void waitGpu();
    void waitGpu(std::size_t fenceIdx);
    void signalGpu();
    void signalGpu(std::size_t fenceIdx);

    /**
     * @brief Alters the fence.
     * @details It just alters the fence index to next one and doesn't block the caller thread.    
     * @note Currently there are two fences and the fence index is 0 or 1.
     * @see Core::waitForGpu
     */
    void alterFence() NOEXCEPT;
    /**
     * @brief Alters the fence.
     * @param idx The index of the fence.
     * @throws gfx::Exception When the fence index is invalid.
     * @details It just alters the fence index to the specified one and doesn't block the caller thread.    
     * @note Currently there are two fences and the fence index is 0 or 1.
     * @see Core::waitForGpu
     */
    void alterFence(std::size_t idx);

    /**
     * @brief Queries the root signature from the Core.
     * @param idx The index of the root signature.
     * @return `wrl::ComPtr<ID3D12RootSignature>` The root signature.
     * @throws gfx::Exception When the root signature for the specified index is not found.
     * @note The root signature for `idx` must be registered to the Core.
     * @see Core::addRoot Core::containsRoot
     */
    wrl::ComPtr<ID3D12RootSignature> root(RootIdx idx) const {
        if (auto pos = roots_.find(idx); pos != roots_.end()) {
            return pos->second;
        }
        throw GFX_EXCEPT_CLASS(d3d12::Core::RootNotFound, idx);
    }
    /**
     * @brief Registers a root signature to the Core.
     * @param idx The index of the root signature, which becomes the key of the root signature.
     * @param pRoot The root signature to register.
     * @note The root signature for `pRoot` can be easily acquired via using root presets.     
     * If you want your own root signature, you have to create it mannually.
     * @see Core::root Core::containsRoot RootPreset makeRootPreset rootName
     */
    void addRoot(RootIdx idx, wrl::ComPtr<ID3D12RootSignature> pRoot) {
        roots_[idx] = std::move(pRoot);
    }
    /**
     * @brief Queries whether the Core contains the root signature for the specified index.
     * @param idx The index of the root signature.
     * @return `bool` Whether the Core contains the root signature for the specified index.
     */
    bool containsRoot(const RootIdx& idx) const NOEXCEPT {
        return roots_.contains(idx);
    }
    /**
     * @brief Registers a temporary upload buffer to the Core.
     * @param idx The index of the temporary upload buffer, which becomes the key of the temporary upload buffer.
     * @param pUpBuf The temporary upload buffer to register.
     * @note The temporary upload buffer for `pUpBuf` can be easily acquired via using createUpBuf.
     * @see Core::tmpUpBuf Core::popTmpUpBuf Core::popTmpUpBufs createUpBuf createDefBuf
     */
    void addTmpUpBuf(UpBufIdx idx, wrl::ComPtr<ID3D12Resource> pUpBuf = nullptr) {
        upBufs_[idx] = std::move(pUpBuf);
    }
    /**
     * @brief Pops the temporary upload buffer for the specified index.     
     * It is recommended to pop the temporary upload buffer if it's use is done to save memory.
     * @param idx The index of the temporary upload buffer.
     * @throws gfx::Exception When the temporary upload buffer for the specified index is not found.
     * @note The temporary upload buffer for `idx` must be registered to the Core.
     * @see Core::addTmpUpBuf Core::popTmpUpBufs createUpBuf createDefBuf
     */
    void popTmpUpBuf(const UpBufIdx& idx) {
        if (auto pos = upBufs_.find(idx); pos != upBufs_.end()) {
            upBufs_.erase(pos);
            return;
        }

        throw GFX_EXCEPT_CLASS(d3d12::Core::UpBufIdxNotFound, idx);
    }
    /**
     * @brief Pops all the temporary upload buffers.
     * @see Core::addTmpUpBuf Core::popTmpUpBuf createUpBuf createDefBuf
     */
    void popTmpUpBufs() NOEXCEPT {
        upBufs_.clear();
    }

    /**
     * @brief Queries the temporary upload buffer for the specified index.
     * @param idx The index of the temporary upload buffer.
     * @return `wrl::ComPtr<ID3D12Resource>&` The temporary upload buffer.
     * @throws gfx::Exception When the temporary upload buffer for the specified index is not found.
     * @note The temporary upload buffer for `idx` must be registered to the Core.
     * @see Core::addTmpUpBuf Core::popTmpUpBuf
     */
    wrl::ComPtr<ID3D12Resource>& tmpUpBuf(const UpBufIdx& idx) {
        if (upBufs_.contains(idx)) {
            return upBufs_.at(idx);
        }
        throw GFX_EXCEPT_CLASS(d3d12::Core::UpBufIdxNotFound, idx);
    }

    void addShader(ShaderIdx idx, std::unique_ptr<Shader>&& shader) {
        shaders_[idx] = std::move(shader);
    }

    /**
     * @brief Queries the Shader for the specified index.
     * @param idx The index of the Shader.
     * @return `const Shader&` The Shader.
     * @throws gfx::Exception When the Shader for the specified index is not found.
     * @note The Shader for `idx` must be registered to the Core.
     * @see Core::addShader Core::popShader Core::containsShader Shader
     */
    Shader& shader(const ShaderIdx& idx) {
        if (shaders_.contains(idx)) {
            return *shaders_.at(idx);
        }
        throw GFX_EXCEPT_CLASS(d3d12::Core::ShaderIdxNotFound, idx);
    }
    /**
     * @brief Queries whether the Core contains the Shader for the specified index.
     * @param idx The index of the Shader.
     * @return `bool` Whether the Core contains the Shader for the specified index.
     */
    bool containsShader(const ShaderIdx& idx) const NOEXCEPT {
        return shaders_.contains(idx);
    }
    /**
     * @brief Pops the Shader for the specified index.     
     * @param idx The index of the Shader.
     * @throws gfx::Exception When the Shader for the specified index is not found.
     * @note The Shader for `idx` must be registered to the Core.
     * @see Core::addShader Core::containsShader
     */
    void popShader(const ShaderIdx& idx) {
        if (auto pos = shaders_.find(idx); pos != shaders_.end()) {
            shaders_.erase(pos);
            return;
        }
        throw GFX_EXCEPT_CLASS(d3d12::Core::ShaderIdxNotFound, idx);
    }
    /**
     * @brief Registers an InputLayout to the Core.
     * @param idx The index of the InputLayout, which becomes the key of the InputLayout.
     * @param inputLayout The InputLayout to register.
     * @note The InputLayout constructor is already quite handy, but it can be acquired easier via using input layout presets.
     * @see Core::inputLayout Core::popInputLayout Core::containsInputLayout InputLayout     
     * makeInputLayoutPreset inputLayoutName
     */
    void addInputLayout(InputLayoutIdx idx, InputLayout inputLayout) {
        inputLayouts_[idx] = std::move(inputLayout);
    }

    /**
     * @brief Queries the InputLayout for the specified index.
     * @param idx The index of the InputLayout.
     * @return `const InputLayout&` The InputLayout.
     * @throws gfx::Exception When the InputLayout for the specified index is not found.
     * @note The InputLayout for `idx` must be registered to the Core.
     * @see Core::addInputLayout Core::popInputLayout Core::containsInputLayout InputLayout
     */
    const InputLayout& inputLayout(const InputLayoutIdx& idx) const {
        if (inputLayouts_.contains(idx)) {
            return inputLayouts_.at(idx);
        }
        throw GFX_EXCEPT_CLASS(d3d12::Core::InputLayoutIdxNotFound, idx);
    }

    /**
     * @brief Queries whether the Core contains the InputLayout for the specified index.
     * @param idx The index of the InputLayout.
     * @return `bool` Whether the Core contains the InputLayout for the specified index.
     */
    bool containsInputLayout(const InputLayoutIdx& idx) const NOEXCEPT {
        return inputLayouts_.contains(idx);
    }

    /**
     * @brief Pops the InputLayout for the specified index.
     * @param idx The index of the InputLayout.
     * @throws gfx::Exception When the InputLayout for the specified index is not found.
     * @note The InputLayout for `idx` must be registered to the Core.
     * @see Core::addInputLayout Core::containsInputLayout
     */
    void popInputLayout(const InputLayoutIdx& idx) {
        if (auto pos = inputLayouts_.find(idx); pos != inputLayouts_.end()) {
            inputLayouts_.erase(pos);
            return;
        }
        throw GFX_EXCEPT_CLASS(d3d12::Core::InputLayoutIdxNotFound, idx);
    }

    void addDescHeap(DescHeapIdx idx, const DescriptorHeap& heap) {
        descriptorHeaps_[idx] = heap;
    }

    void addDescHeap(DescHeapIdx idx, DescriptorHeap&& heap) {
        descriptorHeaps_[idx] = std::move(heap);
    }

    DescriptorHeap& descHeap(const DescHeapIdx& idx) {
        if (descriptorHeaps_.contains(idx)) {
            return descriptorHeaps_.at(idx);
        }
        throw GFX_EXCEPT("DescriptorHeap not found.");
    }

    void popDescHeap(const DescHeapIdx& idx) {
        if (auto pos = descriptorHeaps_.find(idx); pos != descriptorHeaps_.end()) {
            descriptorHeaps_.erase(pos);
            return;
        }
        throw GFX_EXCEPT("DescriptorHeap not found.");
    }

    bool containsDescHeap(const DescHeapIdx& idx) const NOEXCEPT {
        return descriptorHeaps_.contains(idx);
    }

private:
    // TODO: make it return multiple adapters enumerated.
    wrl::ComPtr<IDXGIAdapter1> enumAdapters();
    void createDevice(IDXGIAdapter1* pAdapter);
    void createCommandQueueAndLists(ID3D12Device* pDevice);
    void createFenceAndEvent(ID3D12Device* pDevice);

    const CmdListPool::Element fetchCmdList() {
        return gfxCmdListPool_.fetch();
    }

    void pushCmdList(CmdListPool::Element elem) {
        gfxCmdListPool_.push(std::move(elem));
    }

    wrl::ComPtr<ID3D12CommandQueue> cmdQ() const NOEXCEPT {
        return pCmdQ_;
    }

    static wrl::ComPtr<IDXGIFactory4> spFactory;
    static std::size_t sCmdListPoolSize;
    static std::size_t sFenceCnt;

    CmdListPool gfxCmdListPool_;
    std::map<RootIdx, wrl::ComPtr<ID3D12RootSignature>> roots_;
    std::map<UpBufIdx, wrl::ComPtr<ID3D12Resource>> upBufs_;
    std::map<ShaderIdx, std::unique_ptr<Shader>> shaders_;
    std::map<InputLayoutIdx, InputLayout> inputLayouts_;
    std::map<DescHeapIdx, DescriptorHeap> descriptorHeaps_;
    wrl::ComPtr<ID3D12Device> pDevice_;
    wrl::ComPtr<ID3D12CommandQueue> pCmdQ_;
    std::vector< wrl::ComPtr<ID3D12Fence> > fences_;
    std::vector<UINT64> fenceValues_;
    std::vector<HANDLE> fenceEvents_;
    std::size_t fenceIdx_ = 0;
};

/**
 * @brief The render context for D3D12.    
 * It retreives `wrl::ComPtr<ID3D12GraphicsCommandList>` object through D3D12RenderContext::cast.    
 * And it can be queried for a Shader through D3D12RenderContext::shader,     
 * which is needed for renderer to bind the shader to the command list.
 * @see IRenderContext RenderContextType Shader
 */
class D3D12RenderContext : public IRenderContext {
public:
    D3D12RenderContext(Core& core)
        : pCore_(&core), pCmdList_(), pCmdAlloc_(), pCmdQ_(core.cmdQ()) {
        auto pair = core.fetchCmdList();
        pCmdList_ = std::move(pair.pCmdList);
        pCmdAlloc_ = std::move(pair.pCmdAlloc);
    }

    ~D3D12RenderContext() {
        pCore_->pushCmdList({ std::move(pCmdList_), std::move(pCmdAlloc_) });
    }

    /**
     * @brief Check if the render context is castable to the given context type.
     * @param contextType The context type to check.
     * @return `true` if `contextType` is RenderContextType::D3D12, `false` otherwise.
     * @see RenderContextType D3D12RenderContext::cast
     */
    bool castableTo(RenderContextType contextType) const override;
    /**
     * @brief Cast the render context to the given context type.
     * @param contextType The context type to cast.
     * @return `std::any` The casted object which contains `wrl::ComPtr<ID3D12GraphicsCommandList>`.
     * @see RenderContextType D3D12RenderContext::castableTo
     * @note `contextType` must be RenderContextType::D3D12.
     */
    std::any cast(RenderContextType contextType) override;

    /**
     * @brief Queries the Shader for the specified index.
     * @param idx The index of the Shader.
     * @return `const Shader&` The Shader.
     * @note The Shader for `idx` must be registered to the Core.
     * @see Core::addShader Core::popShader Core::containsShader Shader
     * @note The Shader for `idx` must be registered to the Core.
     */
    Shader& shader(const Core::ShaderIdx& idx) const {
        return pCore_->shader(idx);
    }

    void preRender() override;
    void postRender() override;

private:
    Core* pCore_;
    wrl::ComPtr<ID3D12GraphicsCommandList> pCmdList_;
    wrl::ComPtr<ID3D12CommandAllocator> pCmdAlloc_;
    wrl::ComPtr<ID3D12CommandQueue> pCmdQ_;
};

#ifdef ENABLE_D3D12_WINDOW
class WindowAttorney {
public:
    template <class Traits>
    friend class Window;

private:
    static void* factory(Core& core) {
        return core.spFactory.Get();
    }

    static void* cmdQ(Core& core) {
        return core.pCmdQ_.Get();
    }

    static void* device(Core& core) {
        return core.pDevice_.Get();
    }
};

/**
 * @brief A class for fetching the device object from the Core.     
 * The friend-attorney pattern is applied.
 */
class DeviceFetcher {
public:
    static void* device(Core& core) {
        return core.pDevice_.Get();
    }
};

/**
 * @brief A window for D3D12 which can be used as a render target.
 * It is responsible for providing the render target views and the depth stencil views of swap chain's back buffers.    
 * It builds the render target view and the depth stencil view on Core's corresponding heaps when Window::open is called.    
 * 
 * To acquire the render target view and the depth stencil view,     
 * cast the window to RenderTargetType::D3D12 and RenderTargetType::D3D12_DEPTH, respectively.
 * @tparam Traits The traits class for the window.
 * @details Window::preRender sets resource transition barrier from present to render target,    
 * and set viewport and scissor rect.    
 * 
 * Window::postRender sets resource transition barrier from render target to present.
 * @see IRenderTarget D3DWindow
 */
template <class Traits>
class Window : public D3DWindow<Traits>, public IRenderTarget {
protected:
    /**
     * @brief Get the DXGI factory object from the Core.
     * @param core The Core.
     * @return `void*` The DXGI factory object, safely castable to `IDXGIFactory4*`.
     * @see DXFactory Core
     */
    void* getFactoryFromCore(Core& core) {
        return WindowAttorney::factory(core);
    }

    /**
     * @brief Get the command queue object from the Core.
     * @param core The Core.
     * @return `void*` The command queue object, safely castable to `ID3D12CommandQueue*`.
     * @see Core
     */
    void* getCmdQFromCore(Core& core) {
        return WindowAttorney::cmdQ(core);
    }

    /**
     * @brief Get the device object from the Core.
     * @param core The Core.
     * @return `void*` The device object, safely castable to `ID3D12Device*`.
     * @see Core
     */
    void* getDeviceFromCore(Core& core) {
        return WindowAttorney::device(core);
    }

public:
    template <Win32::Win32Char T>
    friend struct BasicD3D12WTraits;

    static const Core::DescHeapIdx offscreenHeapIdx;
    static const Core::DescHeapIdx offscreenDepthHeapIdx;

    static const std::array<float, 4> clearColor;

    using MyBase = D3DWindow<Traits>;
    using MyChar = typename Traits::MyChar;
    using MyString = typename Traits::MyString;
    using MyStringView = typename Traits::MyStringView;
    using MyBase::nativeHandle;
    using MyBase::defWndName;
    using MyBase::defWndFrame;

    Window()
        : backBuffers_(), offscreenBuffers_(), depthBuffers_(1),
        postRenderFunc_(&Window<Traits>::initialPostRender), pFirstRtv_(), pFirstDsv_(),
        rtvIdx_(0), rtvStride_(0), dsvStride_(0) {}

    /**
     * @brief Opens the window with the default window name which specified in Win32::Window::defWndName    
     * and default window frame which specified in Win32::Window::defWndFrame.
     * @param core The Core.
     * @param backBufCnt The number of back buffers, the default value is D3DWindow::defBackBufCnt.
     * @see Win32::Window::defWndName Win32::Window::defWndFrame    
     * D3DWindow::createDepthBuffers D3DWindow::buildRtv D3DWindow::buildDsv
     */
    void open(Core& core, std::size_t backBufCnt = MyBase::defBackBufCnt) {
        open(core, defWndName(), backBufCnt);
    }
    /**
     * @brief Opens the window with the specified window frame.     
     * The window name is set to the default window name which specified in Win32::Window::defWndName.
     * @param core The Core.
     * @param wndFrame The window frame.
     * @param backBufCnt The number of back buffers, the default value is D3DWindow::defBackBufCnt.
     * @see Win32::Window::defWndName D3DWindow::createDepthBuffers D3DWindow::buildRtv D3DWindow::buildDsv
     */
    void open(Core& core, const Win32::WndFrame& wndFrame, std::size_t backBufCnt = MyBase::defBackBufCnt) {
        open(core, defWndName(), wndFrame, backBufCnt);
    }
    /**
     * @brief Opens the window with the specified window name.    
     * The window frame becomes the default window frame which specified in Win32::Window::defWndFrame.
     * @param core The Core.
     * @param wndName The window name.
     * @param backBufCnt The number of back buffers, the default value is D3DWindow::defBackBufCnt.
     * @see Win32::Window::defWndFrame D3DWindow::createDepthBuffers D3DWindow::buildRtv D3DWindow::buildDsv
     */
    void open(Core& core, MyStringView wndName, std::size_t backBufCnt = MyBase::defBackBufCnt) {
        open(core, wndName, defWndFrame(), backBufCnt);
    }

    // TODO: replace versioned type with type aliases
    /**
     * @brief Opens the window with the specified window name and frame.
     * @param core The Core.
     * @param wndName The window name.
     * @param wndFrame The window frame.
     * @param backBufCnt The number of back buffers, the default value is D3DWindow::defBackBufCnt.
     * @details It builds render target views and depth stencil views on Core's corresponding heaps     
     * using Window::createDepthBuffers, Window::buildRtv, and Window::buildDsv.
     * @see D3DWindow::createDepthBuffers D3DWindow::buildRtv D3DWindow::buildDsv
     */
    void open(Core& core, MyStringView wndName, const Win32::WndFrame& wndFrame, std::size_t backBufCnt = MyBase::defBackBufCnt) {
        MyBase::open( static_cast<IDXGIFactory2*>( WindowAttorney::factory(core) ),
            static_cast<ID3D12CommandQueue*>( WindowAttorney::cmdQ(core) ),
            wndName, wndFrame, backBufCnt
        );
        init(core);
    }

    void buildRtv(Core& core, std::size_t backBufCnt = MyBase::defBackBufCnt);
    void buildDsv(Core& core);
    void createRTBuffers(ID3D12Device* pDevice);
    /**
     * @brief Creates depth buffers for the swap chain's back buffers internally.
     * @param pDevice The device which is going to be used to create the depth buffers.
     * @details It creates the depth buffer with the width and height of the client area of the window,    
     * and the format of `DXGI_FORMAT_D24_UNORM_S8_UINT`.
     * @see Win32::Window::client
     */
    void createDepthBuffers(ID3D12Device* pDevice);

    /**
     * @brief Check if the render target is castable to the given render target type.
     * @param rentarType The render target type to check.
     * @return `true` if `rentarType` is RenderTargetType::D3D12 or RenderTargetType::D3D12_DEPTH, `false` otherwise.
     * @see RenderTargetType
     */
    bool castableTo(RenderTargetType rentarType) const override;
    /**
     * @brief Cast the render target to the given render target type.
     * @param rentarType The render target type to cast.
     * @return `std::any` The casted object which contains `D3D12_CPU_DESCRIPTOR_HANDLE` for either render target view or depth stencil view    
     * depending on the `rentarType`.
     * @see RenderTargetType
     * @note `rentarType` must be RenderTargetType::D3D12 or RenderTargetType::D3D12_DEPTH.
     */
    std::any cast(RenderTargetType rentarType) override;
    // TODO: CPU - GPU synchronization
    void clear(IRenderContext& renderContext) override;
    /**
     * @brief Sets the resource transition barrier from present to render target, and sets viewport and scissor rect.
     * @param renderContext The render context.
     * @see IRenderContext D3D12RenderContext
     * @note `renderContext` must be D3D12RenderContext.
     */
    void preRender(IRenderContext& renderContext) override;
    /**
     * @brief Sets the resource transition barrier from render target to present.
     * @param renderContext The render context.
     * @see IRenderContext D3D12RenderContext
     * @note `renderContext` must be D3D12RenderContext.
     */
    void postRender(IRenderContext& renderContext) override;

private:
    using PostRenderFunc = void(Window<Traits>::*)(ID3D12GraphicsCommandList*);

    void initialPostRender(ID3D12GraphicsCommandList* pCmdList);
    void regularPostRender(ID3D12GraphicsCommandList* pCmdList);

    void preResizeBuffers(ICore& core) override {
        auto pd3d12Core = dynamic_cast<Core*>(&core);
        if (!pd3d12Core) {
            throw GFX_EXCEPT("The Core is not D3D12.");
        }

        backBuffers_.clear();
        depthBuffers_.clear();
    }

    void postResizeBuffers(ICore& core) override {
        auto pd3d12Core = dynamic_cast<Core*>(&core);
        if (!pd3d12Core) {
            throw GFX_EXCEPT("The Core is not D3D12.");
        }

        init(*pd3d12Core);
    }

    void init(Core& core) {
        auto pDevice = static_cast<ID3D12Device*>( WindowAttorney::device(core) );

        backBuffers_.resize( this->backBufCnt() );
        offscreenBuffers_.resize( this->backBufCnt() );
        depthBuffers_.resize( 1 );

        createRTBuffers( pDevice );
        createDepthBuffers( pDevice );
        buildRtv( core, backBuffers_.size() );
        buildDsv( core );
    }

    std::vector<wrl::ComPtr<ID3D12Resource>> backBuffers_;
    std::vector<wrl::ComPtr<ID3D12Resource>> offscreenBuffers_;
    std::vector<wrl::ComPtr<ID3D12Resource>> depthBuffers_;
    PostRenderFunc postRenderFunc_;
    D3D12_CPU_DESCRIPTOR_HANDLE pFirstRtv_;
    D3D12_CPU_DESCRIPTOR_HANDLE pFirstDsv_;
    std::size_t rtvIdx_;
    UINT rtvStride_;
    UINT dsvStride_;
};

// TODO: make D3DWindow's back buffer count modifiable, and reflect that in here. 
template <class Traits>
void Window<Traits>::buildRtv(Core& core, std::size_t backBufCnt) {
    auto pDevice = static_cast<ID3D12Device*>( WindowAttorney::device(core) );

    if (core.containsDescHeap(offscreenHeapIdx)) {
        core.popDescHeap(offscreenHeapIdx);
    }

    core.addDescHeap(offscreenHeapIdx, DescriptorHeap( pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, backBufCnt ));
    
    auto& heap = core.descHeap(offscreenHeapIdx);

    for (auto i = 0; i < backBufCnt; ++i) {
        heap.pushRtv(pDevice, offscreenBuffers_[i].Get());
    }

    pFirstRtv_ = heap.cpuHandle();
    rtvStride_ = static_cast<UINT>( heap.stride() );
}

// TODO: deal with multiple depth stencils.
template <class Traits>
void Window<Traits>::buildDsv(Core& core) {
    auto pDevice = static_cast<ID3D12Device*>( WindowAttorney::device(core) );

    if (core.containsDescHeap(offscreenDepthHeapIdx)) {
        core.popDescHeap(offscreenDepthHeapIdx);
    }

    core.addDescHeap(offscreenDepthHeapIdx, DescriptorHeap( pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1 ));

    auto& heap = core.descHeap(offscreenDepthHeapIdx);

    heap.pushDsv(pDevice, depthBuffers_[0].Get());

    pFirstDsv_ = heap.cpuHandle();
    dsvStride_ = static_cast<UINT>( heap.stride() );
}

template <class Traits>
void Window<Traits>::createRTBuffers(ID3D12Device* pDevice) {
    // create back buffer resources
    for (auto i = 0u; i < backBuffers_.size(); ++i) {
        this->pSwapChain_->GetBuffer(i, __uuidof(ID3D12Resource), &backBuffers_[i]);
    }

    // create offscreen buffer resources
    for (auto i = 0u; i < offscreenBuffers_.size(); ++i) {
        auto backBufDesc = backBuffers_[i]->GetDesc();

        auto offscreenDesc = D3D12_RESOURCE_DESC{
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0,
            .Width = backBufDesc.Width,
            .Height = backBufDesc.Height,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = backBufDesc.Format,
            .SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },   // backBufDesc.SampleDesc?
            .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
            .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
        };

        auto heapProp = D3D12_HEAP_PROPERTIES{
            .Type = D3D12_HEAP_TYPE_DEFAULT,
            .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
            .CreationNodeMask = 0,
            .VisibleNodeMask = 0
        };

        const auto cv = D3D12_CLEAR_VALUE{
            .Format = backBufDesc.Format,
            .Color = { clearColor[0], clearColor[1], clearColor[2], clearColor[3] }
        };

        DX_THROW_FAILED( pDevice->CreateCommittedResource(
            &heapProp, D3D12_HEAP_FLAG_NONE, &offscreenDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &cv, __uuidof(ID3D12Resource), &offscreenBuffers_[i]
        ) );
    }
}

template <class Traits>
void Window<Traits>::createDepthBuffers(ID3D12Device* pDevice) {
    auto depthDesc = D3D12_RESOURCE_DESC{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = static_cast<UINT>(this->client().width),
        .Height = static_cast<UINT>(this->client().height),
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        .SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    };

    auto heapProp = D3D12_HEAP_PROPERTIES{
        .Type = D3D12_HEAP_TYPE_DEFAULT,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0,
        .VisibleNodeMask = 0
    };

    auto cv = D3D12_CLEAR_VALUE{
        .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        .DepthStencil = { .Depth = 1.0f, .Stencil = 0u }
    };

    DX_THROW_FAILED( pDevice->CreateCommittedResource(
        &heapProp, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, __uuidof(ID3D12Resource), &depthBuffers_[0]
    ) );
}

template <class Traits>
bool Window<Traits>::castableTo(RenderTargetType rentarType) const {
    return rentarType == RenderTargetType::D3D12 || rentarType == RenderTargetType::D3D12_DEPTH;
}

template <class Traits>
std::any Window<Traits>::cast(RenderTargetType rentarType) {
    switch (rentarType) {
    case RenderTargetType::D3D12:
        return D3D12_CPU_DESCRIPTOR_HANDLE{
            .ptr = pFirstRtv_.ptr + rtvIdx_ * rtvStride_
        };   
    case RenderTargetType::D3D12_DEPTH:
        return pFirstDsv_;
    default:
        throw GFX_EXCEPT("Cannot cast to the requested render target type.");
    }
}

template <class Traits>
void Window<Traits>::clear(IRenderContext& renderContext) {
    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        renderContext.cast(RenderContextType::D3D12)
    );

    auto bufIdx = rtvIdx_;
    auto pRtv = D3D12_CPU_DESCRIPTOR_HANDLE{
        .ptr = pFirstRtv_.ptr + bufIdx * rtvStride_
    };

    DX_THROW_FAILED_VOID( pCmdList->ClearRenderTargetView(
        pRtv, clearColor.data(), 0, nullptr
    ) );

    DX_THROW_FAILED_VOID( pCmdList->ClearDepthStencilView(
        pFirstDsv_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0, nullptr
    ) );
}

template <class Traits>
void Window<Traits>::preRender(IRenderContext& renderContext) {
    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        renderContext.cast(RenderContextType::D3D12)
    );

    // TODO: set proper viewport
    D3D12_VIEWPORT tmpViewports[] = {
        D3D12_VIEWPORT{
            .TopLeftX = 0.0f,
            .TopLeftY = 0.0f,
            .Width = static_cast<float>(this->client().width),
            .Height = static_cast<float>(this->client().height),
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f
        }
    };

    D3D12_RECT tmpScissorRects[] = {
        D3D12_RECT{
            .left = 0,
            .top = 0,
            .right = static_cast<LONG>(this->client().width),
            .bottom = static_cast<LONG>(this->client().height)
        }
    };

    pCmdList->RSSetViewports(1, tmpViewports);
    pCmdList->RSSetScissorRects(1, tmpScissorRects);
}

template <class Traits>
void Window<Traits>::postRender(IRenderContext& renderContext) {
    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        renderContext.cast(RenderContextType::D3D12)
    );

    (this->*postRenderFunc_)(pCmdList.Get());
}

template <class Traits>
void Window<Traits>::initialPostRender(ID3D12GraphicsCommandList *pCmdList) {
    rtvIdx_ = (rtvIdx_ + 1) % offscreenBuffers_.size();
    postRenderFunc_ = &Window<Traits>::regularPostRender;
}

template <class Traits>
void Window<Traits>::regularPostRender(ID3D12GraphicsCommandList *pCmdList) {
    auto curBackBufIdx = this->pSwapChain_->GetCurrentBackBufferIndex();

    // set current back buffer as copy destination
    auto bar = D3D12_RESOURCE_BARRIER{
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
            .pResource = backBuffers_[curBackBufIdx].Get(),
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
            .StateAfter = D3D12_RESOURCE_STATE_COPY_DEST
        }
    };

    pCmdList->ResourceBarrier(1, &bar);

    // set offscren buffer as copy source
    bar.Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
        .pResource = offscreenBuffers_[rtvIdx_].Get(),
        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
        .StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE
    };

    pCmdList->ResourceBarrier(1, &bar);

    // perform copy
    pCmdList->CopyResource(
        backBuffers_[curBackBufIdx].Get(),
        offscreenBuffers_[rtvIdx_].Get()
    );

    // reset current back buffer as present
    bar.Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
        .pResource = backBuffers_[curBackBufIdx].Get(),
        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
        .StateAfter = D3D12_RESOURCE_STATE_PRESENT
    };

    pCmdList->ResourceBarrier(1, &bar);

    // reset offscreen buffer as render target
    bar.Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
        .pResource = offscreenBuffers_[rtvIdx_].Get(),
        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        .StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE,
        .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET
    };

    pCmdList->ResourceBarrier(1, &bar);

    rtvIdx_ = (rtvIdx_ + 1) % offscreenBuffers_.size();
}

template <class Traits>
const std::array<float, 4> Window<Traits>::clearColor = { 0.0f, 0.2f, 0.4f, 1.0f };

template <class Traits>
const Core::DescHeapIdx Window<Traits>::offscreenHeapIdx = "Offscreen"s;
template <class Traits>
const Core::DescHeapIdx Window<Traits>::offscreenDepthHeapIdx = "Depth"s;

/**
 * @brief The basic traits for D3D12 window.    
 * Besides the BasicD3D12Traits functionalities, it provides the window class name for Window.    
 * If you need customization from this class, you can derive from it and override the static member functions.
 * @tparam `T` The character type.
 * @see BasicD3DWTraits Window D3DWindow Win32::BasicWindowTraits
 */
template <Win32::Win32Char T>
struct BasicD3D12WTraits : public BasicD3DWTraits<T> {
    using MyWindow = Window<BasicD3D12WTraits>;
    using MyBase = BasicD3DWTraits<T>;
    using MyChar = T;
    using MyString = std::basic_string<MyChar>;
    using MyStringView = std::basic_string_view<MyChar>;

    /**
     * @brief Get the window class name.
     * @return `const MyStringView` The window class name `"D3D12W"`.
     */
    static constexpr const MyStringView clsName() NOEXCEPT {
        if constexpr (std::is_same_v<MyChar, CHAR>) {
            return "D3D12W";
        }
        else /* WCHAR */ {
            return L"D3D12W";
        }
    }
};

#endif  // ENABLE_D3D12_WINDOW

}   // namespace d3d12

}   // namespace gfx

#endif // __D3D12CORE_HPP