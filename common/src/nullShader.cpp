#include "nullShader.hpp"

#include "gfxExcept.hpp"

namespace gfx {

namespace d3d12 {

NullShader::NullShader(Core& core)
    : Shader() {
    auto builder = SimpleShaderBuilder();
    builder.code( Type::Vertex, loadCSO(compiledShaderPath/"nullShader_vs.cso") );
    builder.code( Type::Pixel, loadCSO(compiledShaderPath/"nullShader_ps.cso") );

    auto pRoot = core.root( rootName() );
    auto pDevice = static_cast<ID3D12Device*>( DeviceFetcher::device(core) );

    pushInputLayout(InputLayout());
    pushProtocol(rp::Protocol::Null);

    builder.setRoot(pRoot).build(pDevice, *this, 0u);
    builder.wireframe().build(pDevice, *this, 1u);
}

void NullShader::draw( IRenderContext& ctx, const IScene& scene,
    IRenderTarget& target, rp::Protocol protocol
) const {
    if (!supports(protocol)) {
        throw GFX_EXCEPT("The protocol is not supported.");
    }
}

}   // namespace d3d12

}   // namespace gfx