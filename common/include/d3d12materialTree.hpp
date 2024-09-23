#ifndef __MaterialTree_HPP
#define __MaterialTree_HPP

#include "d3d12material.hpp"

#include <vector>

namespace gfx {

namespace d3d12 {

class MaterialTree {
public:
    void addMaterial(const Material& material) { materials_.push_back(material); }
    void addMaterial(Material&& material) { materials_.push_back(std::move(material)); }
    const auto& materials() const { return materials_; }
    auto& materials() { return materials_; }

    void addChild(const MaterialTree& child) { children_.push_back(child); }
    void addChild(MaterialTree&& child) { children_.push_back(std::move(child)); }

    const auto& children() const { return children_; }
    auto& children() { return children_; }

private:
    std::vector<Material> materials_;
    std::vector<MaterialTree> children_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif  // __MaterialTree_HPP