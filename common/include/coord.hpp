#ifndef __COORD_HPP
#define __COORD_HPP

#define DXMATH_VEC_UTIL
#define DXMATH_MAT_UTIL
#define DXMATH_QUAT_UTIL

#include "mathUtil.hpp"

namespace gfx {

namespace coord {

class System {
public:
    System()
        : xform_(), parent_(nullptr) {}

    void setParent(const System* parent) {
        parent_ = parent;
    }

    void MU_CALLCONV accXform(mu::Mat4x4 xform) {
        xform_ *= xform;
    }

    friend System& MU_CALLCONV operator<<(System& cs, mu::Mat4x4 xform) {
        cs.accXform(xform);
        return cs;
    }

    mu::Mat4x4 MU_CALLCONV localXform() const {
        return xform_;
    }

    mu::Mat4x4 MU_CALLCONV totalXform() const {
        return parent_ ? parent_->totalXform() * xform_ : xform_;
    }

private:
    mu::Mat4x4 xform_;
    const System* parent_;
};

class Pt3 {
public:
    Pt3(const System& sys) : pSys_(&sys), val_() {}
    Pt3(const System& sys, mu::Vec3 val) : pSys_(&sys), val_(val) {}
    Pt3(const System& sys, float x, float y, float z) : pSys_(&sys), val_(x, y, z) {}

    mu::Vec3& get() { return val_; }
    const mu::Vec3& get() const { return val_; }
    void MU_CALLCONV set(mu::Vec3 val) { val_ = val; }
    void MU_CALLCONV set(float x, float y, float z) { val_ = mu::Vec3(x, y, z); }
    void MU_CALLCONV set(Pt3 pt) { val_ = pt.represent(*pSys_); }

    mu::Vec3 MU_CALLCONV represent(const System& sys) const {
        return mu::Vec4(val_, 1.f) * pSys_->totalXform() * mu::inverse(sys.totalXform());
    }

    mu::Vec3 MU_CALLCONV represent() const {
        return val_;
    }

    void migrate(const System& sys) {
        val_ = represent(sys);
        pSys_ = &sys;
    }

private:
    const System* pSys_;
    mu::Vec3 val_;
};

class Vec3 {
public:
    Vec3(const System& sys) : pSys_(&sys), val_() {}
    Vec3(const System& sys, mu::Vec3 val) : pSys_(&sys), val_(val) {}
    Vec3(const System& sys, float x, float y, float z) : pSys_(&sys), val_(x, y, z) {}

    mu::Vec3& get() { return val_; }
    const mu::Vec3& get() const { return val_; }
    void MU_CALLCONV set(mu::Vec3 val) { val_ = val; }
    void MU_CALLCONV set(float x, float y, float z) { val_ = mu::Vec3(x, y, z); }
    void MU_CALLCONV set(Vec3 vec) { val_ = vec.represent(*pSys_); }

    mu::Vec3 MU_CALLCONV represent(const System& sys) const {
        return mu::Vec4(val_, 0.f) * pSys_->totalXform() * mu::inverse(sys.totalXform());
    }

    mu::Vec3 MU_CALLCONV represent() const {
        return val_;
    }

    void migrate(const System& sys) {
        val_ = represent(sys);
        pSys_ = &sys;
    }

private:
    const System* pSys_;
    mu::Vec3 val_;
};


}   // namespace coord

}   // namespace gfx

#endif