#ifndef __DXFACTORY_HPP
#define __DXFACTORY_HPP

#include "gfx.hpp"
#include "dxtarget.hpp"
#include "dxexcept.hpp"

namespace gfx {

/**
 * @brief A wrapper class for the DXGI factory.    
 * Since it is required to have device to create DXGI objects and the device needs adapter to be created,     
 * the creation of DXGI factory is primary as only it can enumerate the adapters.    
 * 
 * So, DXFactory::init must be called before any DXGI objects are created     
 * which means it even has to precede the initialization of the ICore implementation.
 * 
 * It can be released right after the creation of the device (i.e. ICore implementation)     
 * because it's job is only enumerating the adapters and therfore it is not needed anymore.
 */
class DXFactory {
public:
    /**
     * @brief Initializes the DXGI factory.
     * @throw DX_EXCEPT if the initialization fails.
     * @details If ENABLE_DXGI_INFO is defined, it creates the factory with DXGI_CREATE_FACTORY_DEBUG flag.
     * @note DXFactory::init must be called before any DXGI objects are created,    
     * indicating that it has to precede even the creation of the ICore implementation.
     */
    static void init();
    static void cleanup();
    static IDXGIFactory4* get() { return spFactory.Get(); }

private:
    static wrl::ComPtr<IDXGIFactory4> spFactory;
};

}   // namespace gfx

#endif  // __DXFACTORY_HPP