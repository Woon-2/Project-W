#ifndef __GFX_HPP
#define __GFX_HPP

#include <memory>
#include <optional>
#include <map>
#include <any>

#include "generator.hpp"

#include "gfxExcept.hpp"

/**
 * @file gfx.hpp
 */

/**
 * @brief The namespace for all graphics-related classes and functions.    
 * It targeted to be able to support multiple graphics APIs, such as D3D11, D3D12, Vulkan, OpenGL, Metal, and Software.    
 * Based on common pattern of rendering pipeline among graphics APIs, it defines the core interfaces for rendering.    
 * To use API-specific functionalities, it is designed to cast a general interface to an API's concrete class in an uniform syntax.
 * @note currently only D3D12 is supported.
 */
namespace gfx {

/**
 * @brief Enum class for different types of render contexts.
 * @see IRenderContext
 */
enum class RenderContextType {
    // D3D11,
    D3D12,
    // Vulkan,
    // OpenGL,
    // Metal,
    // Software,
};

/**
 * @brief Enum class for different types of render targets.
 * @see IRenderTarget
 */
enum class RenderTargetType {
    // D3D11,
    D3D12,
    D3D12_DEPTH
    // Vulkan,
    // OpenGL,
    // Metal,
    // Software,
};
    
/**
 * @brief Interface for a core rendering system.
 * @details
 * A core's initialization must precede any other initialization of graphics components,     
 * and its cleanup must follow any other cleanup of graphics components.     
 * The application should not have more than one instance for each kind of graphics API.
 *     
 * ICore::preRender and ICore::postRender are optional functions that can be used to perform any operations before and after rendering, respectively.     
 * They're designed to be used for gpu-cpu synchronization and communication, such as updating constant buffers, executing command lists, etc.
 * @see IRenderer IRenderContext IRenderTarget
 */
class ICore {
public:
    ICore() = default;
    virtual ~ICore() = default;
    ICore(const ICore&) = default;
    ICore(ICore&&) noexcept = default;
    ICore& operator=(const ICore&) = default;
    ICore& operator=(ICore&&) noexcept = default;

    /**
     * @brief Initialize the core rendering system.    
     * It must precede any other initialization of graphics components.
     */
    virtual void init() = 0;
    /**
     * @brief Render the `scene` to the `target` using `renderer`.    
     * @param scene The scene to be rendered.
     * @param renderer The renderer to be used for rendering.
     * @param target The target to render the scene to.
     * @note Unlike other arguments, target must be compatible with the core.
     * @see IScene IRenderer IRenderTarget
     */
    virtual void render(const class IScene& scene, const class IRenderer& renderer, class IRenderTarget& target) = 0;
    /**
     * @brief Perform any operations before rendering.    
     * It is designed to be used for gpu-cpu synchronization and communication, such as updating constant buffers, executing command lists, etc.
     * @note It is optional.
     */
    virtual void preRender() {}
    /**
     * @brief Perform any operations after rendering.    
     * It is designed to be used for gpu-cpu synchronization and communication, such as updating constant buffers, executing command lists, etc.
     * @note It is optional.
     */
    virtual void postRender() {}
    /**
     * @brief Cleanup the core rendering system.    
     * It must follow any other cleanup of graphics components.
     */
    virtual void cleanup() = 0;
    /**
     * @brief Create a render context for the core rendering system.
     * @return `std::unique_ptr< class IRenderContext >` A unique pointer to the created render context.
     * @see IRenderContext RenderContextType
     */
    virtual std::unique_ptr< class IRenderContext > createContext() = 0;
};

/**
 * @brief Interface for the renderer.
 * @note A renderer is independent of the core rendering system, which means the implementation should be compatible with any rendering API.    
 * @see ICore IRenderContext IRenderTarget
 */
class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual void init(ICore& core) = 0;
    /**
     * @brief Render the `scene` to the `target` using the `renderContext`.
     * @param scene The scene to be rendered.
     * @param renderContext The render context to be used for rendering.
     * @param target The target to render the scene to.
     * @see IScene IRenderContext IRenderTarget
     * @details
     * It should iterate through the scene and perform draw call for each DrawInfo retreived from the iteration,    
     * calling proper rendering API functions through the render context.
     * @note A renderer and a scene should share and be aware of what the DrawInfo contains and how to interpret it.
     */
    virtual void render(const class IScene& scene, class IRenderContext& renderContext, class IRenderTarget& target) const = 0;
    virtual void cleanup() = 0;
};

/**
 * @brief Interface for a render context.     
 * A render context is used to manipulate the rendering process, such as setting up the pipeline, binding resources, etc.    
 * A renderer consumes a render context to perform rendering operations.     
 * To be used in a renderer, a render context must be casted to a concrete class that supports rendering API functions.
 * @see IRenderer RenderContextType
 */
class IRenderContext {
public:
    IRenderContext() = default;
    virtual ~IRenderContext() = 0;
    IRenderContext(const IRenderContext&) = default;
    IRenderContext(IRenderContext&&) noexcept = default;
    IRenderContext& operator=(const IRenderContext&) = default;
    IRenderContext& operator=(IRenderContext&&) noexcept = default;

    /**
     * @brief Check if the render context is castable to the given context type.
     * @param contextType The context type to check.
     * @return `true` if the render context is castable to the given context type, `false` otherwise.
     * @see RenderContextType IRenderContext::cast
     */
    virtual bool castableTo(RenderContextType contextType) const = 0;
    /**
     * @brief Cast the render context to the given context type.     
     * A renderer consumes a render context to perform rendering operations.     
     * To be used in a renderer, a render context must be casted to a concrete class that supports rendering API functions.    
     * The usage is a little bit tricky, because the casted object is stored in an `std::any` object.     
     * The returned `std::any` object should be casted to a desired class through `std::any_cast<T>` function.
     * @code
     * auto renderContext = core.createContext();
     *
     * if (renderContext->castableTo(RenderContextType::D3D12)) {
     *    auto d3d12Context = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
     *        renderContext->cast(RenderContextType::D3D12)
     *    );
     *   // use d3d12Context
     * }
     * @endcode
     * @param contextType The context type to cast to.
     * @return `std::any` An object that contains the casted render context.
     * @see RenderContextType
     */
    virtual std::any cast(RenderContextType contextType) = 0;
};

/**
 * @brief Interface for a render target.
 * @note unlike renderers, render targets are rendering API-dependent.    
 * So, a render target must be compatible with a core or a render context when it is used with them.
 * @details
 * IRenderTarget::preRender and IRenderTarget::postRender are optional functions that can be used to perform any operations before and after rendering, respectively.    
 * They're designed to be used for clearing the target, uploading initial values, fetching the render result, gpu-cpu synchronization, etc.    
 * 
 * IRenderTarget::preRender should be performed at least after ICore::preRender and before IRenderer::render.    
 * IRenderTarget::postRender should be performed at least after IRenderer::render and before ICore::postRender.    
 * @see ICore IRenderer IRenderContext
 */
class IRenderTarget {
public:
    IRenderTarget() = default;
    virtual ~IRenderTarget() = default;
    IRenderTarget(const IRenderTarget&) = default;
    IRenderTarget(IRenderTarget&&) noexcept = default;
    IRenderTarget& operator=(const IRenderTarget&) = default;
    IRenderTarget& operator=(IRenderTarget&&) noexcept = default;

    /**
     * @brief Check if the render target is castable to the given target type.
     * @param type The target type to check.
     * @return `true` if the render target is castable to the given target type, `false` otherwise.
     * @see RenderTargetType
     */
    virtual bool castableTo(RenderTargetType type) const = 0;
    /**
     * @brief Cast the render target to the given target type.    
     * The usage is a little bit tricky, because the casted object is stored in an `std::any` object.    
     * The returned `std::any` object should be casted to a desired class through `std::any_cast<T>` function.
     * @code
     * if (renderTarget.castableTo(RenderTargetType::D3D12)) {
     *    auto rtvHandle = std::any_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(
     *        renderTarget.cast(RenderTargetType::D3D12)
     *    );
     *    auto dsvHandle = std::any_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(
     *        renderTarget.cast(RenderTargetType::D3D12_DEPTH)
     *    );
     * 
     *    pCmdList->OMSetRenderTargets(1u, &rtvHandle, true, &dsvHandle);
     * }
     * @endcode
     * @param type The target type to cast to.
     * @return `std::any` An object that contains the casted render target.
     * @see RenderTargetType
     */
    virtual std::any cast(RenderTargetType type) = 0;
    /**
     * @brief Perform any operations before rendering.    
     * It is designed to be used for clearing the target, uploading initial values, gpu-cpu synchronization, etc.    
     * If clearing render target always before rendering is not desired,    
     * do not include render target clearing operation in this function.    
     * Instead, implement and use IRenderTarget::clear.
     * @note It is optional.
     */
    virtual void preRender(IRenderContext& renderContext) {}
    /**
     * @brief Perform any operations after rendering.    
     * It is designed to be used for clearing the target, fetching the render result, gpu-cpu synchronization, etc.
     * If clearing render target always after rendering is not desired,    
     * do not include render target clearing operation in this function.    
     * Instead, implement and use IRenderTarget::clear.
     * @note It is optional.
     */
    virtual void postRender(IRenderContext& renderContext) {}
    /**
     * @brief Clear the render target.
     * @param renderContext The render context to be used for clearing the target.
     * @note It is optional.
     */
    virtual void clear(IRenderContext& renderContext) {}
};

// TODO: add global map to track Type and its string representation for debugging
/**
 * @brief Class for storing information to be used in rendering.    
 * A Drawinfo instance contains a map of information,    
 * where the key indicates stored information's type and the value is an `std::any` object which contains the information.    
 * Constructed by a scene and consumed by a renderer, the information is exchanged between them.    
 * 
 * Importantly, each Drawinfo instance is used for a single iteration of rendering, becoming the unit of draw call.
 * @note A renderer and a scene should share and be aware of what the DrawInfo contains and how to interpret it.
 * @see IScene IRenderer
 */
class DrawInfo {
private:
    class AnyWrapper : public std::any {
    public:
        using std::any::any;
        using std::any::operator=;
        
        template <class T>
        T cast() const {
            return std::any_cast<T>(*this);
        }
    };

public:
    using Type = int;

    /**
     * @brief map the given type to the given value.
     * @tparam T The type of the value to be stored.
     * @param type The type index to be mapped.
     * @param value The value to be stored.
     * @note The value is stored in an `std::any` object.
     */
    template <class T>
    void set(Type type, T&& value) {
        info_[type] = std::forward<T>(value);
    }

    /**
     * @brief Get the value of the given type.
     * @tparam T The type of the value to be retrieved.
     * @param type The type index to be retrieved.
     * @return T The value of the given type.
     * @note The value is casted from an `std::any` object.
     */
    template <class T>
    T get(Type type) const {
        return info_.at(type).cast<T>();
    }

    /**
     * @brief Get the value of the given type.
     * @param type The type index to be retrieved.
     * @return DrawInfo::AnyWrapper& The value of the given type, stored in an `std::any` object.
     */
    AnyWrapper& operator[](Type type) {
        return info_[type];
    }
    /**
     * @brief Get the value of the given type.
     * @param type The type index to be retrieved.
     * @return const DrawInfo::AnyWrapper& The value of the given type, stored in an `std::any` object.
     */
    const AnyWrapper& operator[](Type type) const {
        return info_.at(type);
    }

private:
    std::map<Type, AnyWrapper> info_;
};

/**
 * @brief Interface for a scene.
 * A scene is a collection of objects to be rendered.    
 * It is used to generate a sequence of DrawInfo instances, which are consumed by a renderer to perform rendering operations.    
 * Each DrawInfo instance retreived from the iteration function is used for a single iteration of rendering, becoming the unit of draw call.
 * @details The iteration function should be implemented as a coroutine which generates the sequence of DrawInfo instances.    
 * @see IRenderer DrawInfo
 * @note A Scene and a renderer should share and be aware of what the DrawInfo contains and how to interpret it.
 */
class IScene {
public:
    IScene() = default;
    virtual ~IScene() = default;
    IScene(const IScene&) = default;
    IScene(IScene&&) noexcept = default;
    IScene& operator=(const IScene&) = default;
    IScene& operator=(IScene&&) noexcept = default;

    /**
     * @brief Generate a sequence of DrawInfo instances.
     * @return Generator<DrawInfo> An iterable object that generates the sequence of DrawInfo instances.
     * @note This function should be implemented as a coroutine which yields DrawInfo instances.
     */
    virtual Generator<DrawInfo> iteration() const = 0;
};

}   // namespace gfx

#endif // __GFX_HPP