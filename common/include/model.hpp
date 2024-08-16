#ifndef __MODEL_HPP
#define __MODEL_HPP

#define ASSIMP_MATH_UTIL

#include "mesh.hpp"
#include "inputLayout.hpp"

#include "coord.hpp"

#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include <filesystem>

namespace gfx {

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
    Model(const std::string& name)
        : coordSys_(), children_(), meshes_(), name_(name) {}

    std::string_view name() const NOEXCEPT {
        return name_;
    }

    void addChild(const Model& child) {
        children_.push_back(child);
        children_.back().coord().setParent(&coordSys_);
    }

    void addChild(Model&& child) {
        children_.push_back(std::move(child));
        children_.back().coord().setParent(&coordSys_);
    }

    void addChild(Model&& child, mu::Mat4x4 xform) {
        children_.push_back(std::move(child));
        children_.back().coord().setParent(&coordSys_);
        children_.back().coord() << xform;
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
    coord::System coordSys_;
    std::vector<Model> children_;
    std::vector<NamedMesh> meshes_;
    std::string name_;
};

Model loadModel(const std::filesystem::path& path);
Model loadModel(const std::filesystem::path& path, const InputLayout& il);

}   // namespace gfx

#endif // __MODEL_HPP