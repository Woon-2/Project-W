#ifndef __DXFACTORY_HPP
#define __DXFACTORY_HPP

#include "gfx.hpp"
#include "dxtarget.hpp"
#include "dxexcept.hpp"

namespace gfx {

class DXFactory {
public:
    static void init();
    static void cleanup();
    static IDXGIFactory4* get() { return spFactory.Get(); }

private:
    static wrl::ComPtr<IDXGIFactory4> spFactory;
};

}   // namespace gfx

#endif  // __DXFACTORY_HPP