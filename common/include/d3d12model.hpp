#ifndef __D3D12MODEL_HPP
#define __D3D12MODEL_HPP

#include "model.hpp"

#include "d3d12core.hpp"
#include "d3d12mesh.hpp"

#include "coord.hpp"

#include <vector>
#include <string>

namespace gfx {

namespace d3d12 {

class Model {
private:
    struct NamedMesh {
        Mesh mesh;
        std::string name;

        friend auto operator<=>(const NamedMesh& lhs, const NamedMesh& rhs) noexcept {
            return lhs.name <=> rhs.name;
        }
    };

public:
    Model( d3d12::Core& core, d3d12::D3D12RenderContext& ctx, const gfx::Model& model,
        Core::UpBufIdx vbUpIdx, Core::UpBufIdx ibUpIdx
    ) : Model(core, ctx, model, vbUpIdx, ibUpIdx, 0, 0) {}

    void completeInit(d3d12::Core& core) const;

    std::string_view name() const NOEXCEPT {
        return name_;
    }

    void addChild(const Model& child) {
        children_.push_back(child);
    }

    void addChild(Model&& child) {
        children_.push_back(std::move(child));
    }

    Model popChild(std::string_view name);
    const Model& child(std::string_view name) const;

    Model& child(std::string_view name) {
        return const_cast<Model&>( const_cast<const Model*>(this)->child(name) );
    }

    void addMesh(const Mesh& mesh, const std::string& name) {
        meshes_.push_back({mesh, name});
    }

    void addMesh(Mesh&& mesh, const std::string& name) {
        meshes_.push_back({std::move(mesh), name});
    }

    void addMesh(const Mesh& mesh, std::string&& name) {
        meshes_.push_back({mesh, std::move(name)});
    }

    void addMesh(Mesh&& mesh, std::string&& name) {
        meshes_.push_back({std::move(mesh), std::move(name)});
    }

    Mesh popMesh(std::string_view name);
    const Mesh& mesh(std::string_view name) const;

    Mesh& mesh(std::string_view name) {
        return const_cast<Mesh&>( const_cast<const Model*>(this)->mesh(name) );
    }

    void setCoord(const coord::System& coordSys) {
        coordSys_ = coordSys;
    }

    const coord::System& coord() const NOEXCEPT {
        return coordSys_;
    }

    coord::System& coord() NOEXCEPT {
        return coordSys_;
    }

    auto& children() NOEXCEPT {
        return children_;
    }

    const auto& children() const NOEXCEPT {
        return children_;
    }

    auto& meshes() NOEXCEPT {
        return meshes_;
    }

    const auto& meshes() const NOEXCEPT {
        return meshes_;
    }

private:
    Model( d3d12::Core& core, d3d12::D3D12RenderContext& ctx, const gfx::Model& model,
        Core::UpBufIdx vbUpIdx, Core::UpBufIdx ibUpIdx, std::size_t vbSerialIdx, std::size_t ibSerialIdx
    );

    Core::UpBufIdx serializeVbIdx(const Core::UpBufIdx& idx);
    Core::UpBufIdx serializeIbIdx(const Core::UpBufIdx& idx);

    coord::System coordSys_;
    std::vector<Model> children_;
    std::vector<NamedMesh> meshes_;
    std::string name_;
    std::size_t vbSerialIdx_;
    std::size_t ibSerialIdx_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __D3D12MODEL_HPP