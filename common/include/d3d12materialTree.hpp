#ifndef __MaterialTree_HPP
#define __MaterialTree_HPP

#include "d3d12material.hpp"

namespace gfx {

namespace d3d12 {

class MaterialTree {
public:
    MaterialTree(const Material& material = Material())
        : material_(material), children_() {}

    MaterialTree(Material&& material)
        : material_(std::move(material)), children_() {}

    const Material& material() const { return material_; }
    Material& material() { return material_; }
    void setMaterial(const Material& material) { material_ = material; }

    void addChild(const MaterialTree& child) { children_.push_back(child); }
    void addChild(MaterialTree&& child) { children_.push_back(std::move(child)); }

    const auto& children() const { return children_; }
    auto& children() { return children_; }

private:
    Material material_;
    std::vector<MaterialTree> children_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif  // __MaterialTree_HPP