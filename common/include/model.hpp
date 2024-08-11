#ifndef __MODEL_HPP
#define __MODEL_HPP

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

    std::string_view name() const {
        return name_;
    }

    void addChild(const Model& child) {
        children_.push_back(&child);
    }

    void popChild(std::string_view name) {
        std::erase_if( children_, [&name](const Model* child) {
            return child->name() == name;
        } );
    }

    const Model& child(std::string_view name) const;

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

    void popMesh(std::string_view name) {
        std::erase_if( meshes_, [&name](const NamedMesh& nm) {
            return nm.name == name;
        } );
    }

    const Mesh& mesh(std::string_view name) const;

    Mesh& mesh(std::string_view name) {
        return const_cast<Mesh&>( const_cast<const Model*>(this)->mesh(name) );
    }

    void setCoord(const coord::System& coordSys) {
        coordSys_ = coordSys;
    }

    const coord::System& coord() const {
        return coordSys_;
    }

    coord::System& coord() {
        return coordSys_;
    }

private:
    coord::System coordSys_;
    std::vector<const Model*> children_;
    std::vector<NamedMesh> meshes_;
    std::string name_;
};

Model loadModel(const std::filesystem::path& path);
Model loadModel(const std::filesystem::path& path, const InputLayout& il);

}   // namespace gfx

#endif // __MODEL_HPP