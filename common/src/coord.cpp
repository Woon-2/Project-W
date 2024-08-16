#include "coord.hpp"

#include <utility>
#include <algorithm>

namespace gfx {

namespace coord {

System::System(const System& sys)
    : localXform_(sys.localXform_), cachedTotalXform_(sys.cachedTotalXform_), children_(), parent_(sys.parent_) {
    if (parent_) {
        const_cast<System*>(parent_)->children_.push_back(this);
    }
}

System& System::operator=(const System& sys) {
    if (this == &sys) {
        return *this;
    }

    localXform_ = sys.localXform_;
    cachedTotalXform_ = sys.cachedTotalXform_;
    parent_ = sys.parent_;

    if (parent_) {
        const_cast<System*>(parent_)->children_.push_back(this);
    }

    return *this;
}

System::System(System&& sys) noexcept
    : localXform_(std::move(sys.localXform_)),
    cachedTotalXform_(std::move(sys.cachedTotalXform_)),
    children_(std::move(sys.children_)),
    parent_(std::exchange(sys.parent_, nullptr)) {
    for (auto& child : children_) {
        child->parent_ = this;
    }

    if (parent_) {
        auto p = const_cast<System*>(parent_);
        std::erase(p->children_, &sys);
        p->children_.push_back(this);
    }
}

System& System::operator=(System&& sys) noexcept {
    if (this == &sys) {
        return *this;
    }

    localXform_ = std::move(sys.localXform_);
    cachedTotalXform_ = std::move(sys.cachedTotalXform_);
    parent_ = std::exchange(sys.parent_, nullptr);

    for (auto& child : children_) {
        child->parent_ = this;
    }

    if (parent_) {
        auto p = const_cast<System*>(parent_);
        std::erase(p->children_, &sys);
        p->children_.push_back(this);
    }

    return *this;
}

System::~System() {
    for (auto child : children_) {
        child->parent_ = nullptr;
        child->localXform_ = child->localXform_ * cachedTotalXform_;
    }

    if (parent_) {
        auto p = const_cast<System*>(parent_);
        std::erase(p->children_, this);
    }
}

void System::traverse(const mu::Mat4x4& parentXform) NOEXCEPT {
    cachedTotalXform_ = localXform_ * parentXform;

    for (auto& child : children_) {
        child->traverse(cachedTotalXform_);
    }
}

void System::setParent(System* parent) {
    if (parent_) {
        std::erase(const_cast<System*>(parent_)->children_, this);
    }

    if (parent != nullptr) {
        parent->children_.push_back(this);
    }

    parent_ = parent;
}

}   // namespace gfx::coord

}   // namespace gfx