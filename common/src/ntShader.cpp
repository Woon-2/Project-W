#include "ntShader.hpp"

namespace gfx {

namespace d3d12 {

NTShader::NTShader(Core& core)
    : Shader() {
    auto builder = SimpleShaderBuilder();
    builder.code( Type::Vertex, loadCSO(compiledShaderPath/"ntShader_vs.cso") );
    builder.code( Type::Pixel, loadCSO(compiledShaderPath/"ntShader_ps.cso") );

    auto pRoot = core.root( rootName() );
    auto pDevice = static_cast<ID3D12Device*>( DeviceFetcher::device(core) );

    const auto& il = core.inputLayout( inputLayoutName() );

    builder.setInputLayout(il).setRoot(pRoot).build(pDevice, *this, 0u);
    builder.wireframe().build(pDevice, *this, 1u);
}

}   // namespace d3d12

}   // namespace gfx