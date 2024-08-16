#ifndef __GFX_HPP
#define __GFX_HPP

#include <memory>
#include <optional>
#include <map>
#include <any>

#include "generator.hpp"

#include "gfxExcept.hpp"

namespace gfx {
    
enum class RenderContextType {
    // D3D11,
    D3D12,
    // Vulkan,
    // OpenGL,
    // Metal,
    // Software,
};

enum class RenderTargetType {
    // D3D11,
    D3D12,
    D3D12_DEPTH
    // Vulkan,
    // OpenGL,
    // Metal,
    // Software,
};
    
class ICore {
public:
    ICore() = default;
    virtual ~ICore() = default;
    ICore(const ICore&) = default;
    ICore(ICore&&) noexcept = default;
    ICore& operator=(const ICore&) = default;
    ICore& operator=(ICore&&) noexcept = default;

    virtual void init() = 0;
    virtual void render(const class IScene& scene, const class IRenderer& renderer, class IRenderTarget& target) = 0;
    virtual void preRender() {}
    virtual void postRender() {}
    virtual void cleanup() = 0;
    virtual std::unique_ptr< class IRenderContext > createContext() = 0;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual void init(ICore& core) = 0;
    virtual void render(const class IScene& scene, class IRenderContext& renderContext, class IRenderTarget& target) const = 0;
    virtual void cleanup() = 0;
};

class IRenderContext {
public:
    IRenderContext() = default;
    virtual ~IRenderContext() = 0;
    IRenderContext(const IRenderContext&) = default;
    IRenderContext(IRenderContext&&) noexcept = default;
    IRenderContext& operator=(const IRenderContext&) = default;
    IRenderContext& operator=(IRenderContext&&) noexcept = default;

    virtual bool castableTo(RenderContextType contextType) const = 0;
    virtual std::any cast(RenderContextType contextType) = 0;
};

class IRenderTarget {
public:
    IRenderTarget() = default;
    virtual ~IRenderTarget() = default;
    IRenderTarget(const IRenderTarget&) = default;
    IRenderTarget(IRenderTarget&&) noexcept = default;
    IRenderTarget& operator=(const IRenderTarget&) = default;
    IRenderTarget& operator=(IRenderTarget&&) noexcept = default;

    virtual bool castableTo(RenderTargetType type) const = 0;
    virtual std::any cast(RenderTargetType type) = 0;
    virtual void preRender(IRenderContext& renderContext) {}
    virtual void postRender(IRenderContext& renderContext) {}
    virtual void clear(IRenderContext& renderContext) {}
};

// TODO: add global map to track Type and its string representation for debugging
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

    template <class T>
    void set(Type type, T&& value) {
        info_[type] = std::forward<T>(value);
    }

    template <class T>
    T get(Type type) const {
        return info_.at(type).cast<T>();
    }

    AnyWrapper& operator[](Type type) {
        return info_[type];
    }

    const AnyWrapper& operator[](Type type) const {
        return info_.at(type);
    }

private:
    std::map<Type, AnyWrapper> info_;
};

class IScene {
public:
    IScene() = default;
    virtual ~IScene() = default;
    IScene(const IScene&) = default;
    IScene(IScene&&) noexcept = default;
    IScene& operator=(const IScene&) = default;
    IScene& operator=(IScene&&) noexcept = default;

    virtual Generator<DrawInfo> iteration() const = 0;
};

}   // namespace gfx

#endif // __GFX_HPP