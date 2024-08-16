#ifndef __COORD_HPP
#define __COORD_HPP

#define DXMATH_VEC_UTIL
#define DXMATH_MAT_UTIL
#define DXMATH_QUAT_UTIL

#include "mathUtil.hpp"

#include <utility>
#include <vector>
#include <memory>
#include <type_traits>
#include <stack>

#include <cassert>

#include "gfxExcept.hpp"

namespace gfx {

namespace coord {

class System {
public:
    System()
        : localXform_(), cachedTotalXform_(), children_(), parent_(nullptr) {}

    System(const System& sys);
    System& operator=(const System& sys);
    System(System&& sys) noexcept;
    System& operator=(System&& sys) noexcept;

    ~System();

    void accXform(const mu::Mat4x4& xform) NOEXCEPT {
        localXform_ *= xform;
    }

    mu::Mat4x4 MU_CALLCONV localXformTransposed() const NOEXCEPT {
        return mu::transpose(localXform_);
    }

    const mu::Mat4x4& localXform() const NOEXCEPT {
        return localXform_;
    }

    const mu::Mat4x4& xform() const NOEXCEPT {
        return cachedTotalXform_;
    }

    void traverse(const mu::Mat4x4& parentXform = mu::Mat4x4()) NOEXCEPT;
    void setParent(System* parent);

    const System* parent() const NOEXCEPT {
        return parent_;
    }

    void popChild(const System* child) {
        std::erase_if( children_, [child](const auto& c) {
            return c == child;
        } );
    }

    friend System& operator<<(System& cs, const mu::Mat4x4& xform) {
        cs.accXform(xform);
        return cs;
    }

private:
    mu::Mat4x4 localXform_;
    mu::Mat4x4 cachedTotalXform_;
    std::vector<System*> children_;
    mutable const System* parent_;
};

namespace detail {

// TODO: add noexcept expression macro
template <class Conc>
class CoordEntry {
protected:
    const System* pSys_;
    mu::Vec3 val_;

public:
    CoordEntry(const System* pSys) noexcept(NDEBUG)
        : pSys_(pSys), val_() {
        assert(pSys != nullptr);
    }

    CoordEntry(const System* pSys, mu::Vec3 val) noexcept(NDEBUG)
        : pSys_(pSys), val_(val) {
        assert(pSys != nullptr);
    }

    CoordEntry(const System* pSys, float x, float y, float z) noexcept(NDEBUG)
        : pSys_(pSys), val_(x, y, z) {
        assert(pSys != nullptr);  
    }

    mu::Vec3& get() NOEXCEPT { return val_; }
    const mu::Vec3& get() const NOEXCEPT { return val_; }
    void set(const mu::Vec3& val) NOEXCEPT { val_ = val; }
    void set(float x, float y, float z) NOEXCEPT { val_ = mu::Vec3(x, y, z); }

    void MU_CALLCONV set(Conc other) NOEXCEPT {
        val_ = other.represent(*pSys_);
    }

    const mu::Vec3 represent(const System& sys) const NOEXCEPT {
        return static_cast<const Conc*>(this)->representImpl(sys);
    }

    const mu::Vec3 represent() const NOEXCEPT {
        return static_cast<const Conc*>(this)->representImpl();
    }

    void migrate(const System* pSys) noexcept(NDEBUG) {
        assert(pSys != nullptr);
        val_ = represent(*pSys);
        pSys_ = pSys;
    }
};

}   // namespace gfx::coord::detail

class Pt3 : public detail::CoordEntry<Pt3> {
public:
    friend class detail::CoordEntry<Pt3>;
    using detail::CoordEntry<Pt3>::CoordEntry;

private:
    mu::Vec3 MU_CALLCONV representImpl(const System& sys) const NOEXCEPT {
        return mu::Vec4(val_, 1.f) * pSys_->xform() * mu::inverse(sys.xform());
    }

    mu::Vec3 MU_CALLCONV representImpl() const NOEXCEPT {
        return val_;
    }
};

class Vec3 : public detail::CoordEntry<Vec3> {
public:
    friend class detail::CoordEntry<Vec3>;
    using detail::CoordEntry<Vec3>::CoordEntry;

private:
    mu::Vec3 MU_CALLCONV representImpl(const System& sys) const NOEXCEPT {
        return mu::Vec4(val_, 0.f) * pSys_->xform() * mu::inverse(sys.xform());
    }

    mu::Vec3 MU_CALLCONV representImpl() const NOEXCEPT {
        return val_;
    }
};

}   // namespace coord

}   // namespace gfx

#endif