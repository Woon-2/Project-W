cmake_minimum_required(VERSION 3.18)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    if(NOT DEFINED CMAKE_CONFIGURATION_TYPES)
        set(DEFAULT_CONFIGURATION_TYPES "Debug;Release")
        message(STATUS "Setting configuration types to ${DEFAULT_CONFIGURATION_TYPES}
            as none was specified."
        )
        set(CMAKE_CONFIGURATION_TYPES ${DEFAULT_CONFIGURATION_TYPES} CACHE STRING
            "Specify available configuration types"
        )
    endif()

    if(NOT DEFINED CMAKE_BUILD_TYPE)
        set(DEFAULT_BUILD_TYPE "Release")
        message(STATUS "Setting build type to ${DEFAULT_BUILD_TYPE}
            as none was specified."
        )
        set(CMAKE_BUILD_TYPE ${DEFAULT_BUILD_TYPE} CACHE STRING
            "Choose the type of build."
        )
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "${CMAKE_CONFIGURATION_TYPES}"
        )
    endif()

    if (NOT DEFINED CMAKE_INSTALL_PREFIX)
        set(DEFAULT_INSTALL_PATH "${CMAKE_BINARY_DIR}/install")
        message(STATUS "Setting install path to ${DEFAULT_INSTALL_PATH}
            as none was specified."
        )
        set(CMAKE_INSTALL_PREFIX ${DEFAULT_INSTALL_PATH} CACHE PATH
            "Specify the path to install this project"
        )
    endif()
endif()

include(auto_configure)
include(win32_configuration)

# === woon2exception ===
add_library(woon2exception
    src/Woon2Exception.cpp include/Woon2Exception.hpp
)

add_library(common::woon2exception ALIAS woon2exception)

auto_configure(woon2exception PRIVATE)

target_include_directories(woon2exception
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# === win32 ===
add_library(win32
    src/Window.cpp include/Window.hpp include/Window.inl
)

add_library(common::win32 ALIAS win32)

auto_configure(win32 PRIVATE)
config_win32(win32 PUBLIC)

target_include_directories(win32
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(win32
    PRIVATE
        common::woon2exception
)

# === math ===
add_library(mu
    INTERFACE
        include/mathUtil.hpp
)

add_library(common::mu ALIAS mu)

auto_configure(mu INTERFACE)

target_include_directories(mu
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# === coro ===
add_library(coro
    INTERFACE
        include/generator.hpp
        include/resumable.hpp
)

add_library(common::coro ALIAS coro)

auto_configure(coro INTERFACE)

target_include_directories(coro
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# === input ===
add_library(input
    src/mouse.cpp include/mouse.hpp
    src/keyboardXX.cpp include/keyboardXX.hpp
    src/keyboardWin32Adaptor.cpp include/keyboardWin32Adaptor.hpp
    include/mouseWin32Adaptor.hpp
    
    include/config.hpp
)

add_library(common::input ALIAS input)

auto_configure(input PRIVATE)

target_include_directories(input
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(input
    PRIVATE
        common::win32
        common::coro
)

# === memory ===
add_library(mem
    src/alignMem.cpp include/alignMem.hpp
)

add_library(common::mem ALIAS mem)

auto_configure(mem PRIVATE)

target_include_directories(mem
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)


# === gfx ===
add_library(gfx
    src/gfx.cpp include/gfx.hpp
    src/gfxExcept.cpp include/gfxExcept.hpp
    src/dxfactory.cpp include/dxfactory.hpp
    src/dxexcept.cpp include/dxexcept.hpp
    src/d3d12core.cpp include/d3d12core.hpp
    src/d3dwindow.cpp include/d3dwindow.hpp
    src/phongRenderer.cpp include/phongRenderer.hpp
    src/sampleScene.cpp include/sampleScene.hpp
    src/d3d12shader.cpp include/d3d12shader.hpp
    src/rootPresets.cpp include/rootPresets.hpp
    src/d3d12resLow.cpp include/d3d12resLow.hpp
    src/d3d12res.cpp include/d3d12res.hpp
    src/vertex.cpp include/vertex.hpp
    src/mesh.cpp include/mesh.hpp
    src/d3d12mesh.cpp include/d3d12mesh.hpp
    src/inputLayout.cpp include/inputLayout.hpp
    src/d3d12InputLayout.cpp include/d3d12InputLayout.hpp
    src/d3d12Drawers.cpp include/d3d12Drawers.hpp
    src/nullShader.cpp include/nullShader.hpp
    src/solidShader.cpp include/solidShader.hpp
    src/ntShader.cpp include/ntShader.hpp
    src/phongShader.cpp include/phongShader.hpp
    src/inputLayoutPresets.cpp include/inputLayoutPresets.hpp
    src/d3d12InputLayoutPresets.cpp include/d3d12InputLayoutPresets.hpp
    src/model.cpp include/model.hpp
    src/d3d12model.cpp include/d3d12model.hpp
    src/camera.cpp include/camera.hpp
    src/d3d12scene.cpp include/d3d12scene.hpp
    src/coord.cpp include/coord.hpp
    src/shaderRes.cpp include/shaderRes.hpp
    src/drawInfo.cpp include/drawInfo.hpp
    src/gpuMem.cpp include/gpuMem.hpp
    src/renderProtocol.cpp include/renderProtocol.hpp
    src/d3d12texture.cpp include/d3d12texture.hpp
    src/d3d12material.cpp include/d3d12material.hpp
    src/assimpLoader.cpp include/assimpLoader.hpp
    src/assimpLoaderD3d12.cpp include/assimpLoaderD3d12.hpp
    src/d3d12materialTree.cpp include/d3d12materialTree.hpp
    
    include/dxtarget.hpp
    include/gfxPrimitive.hpp
    include/shaderPath.hpp
    include/enumUtil.hpp
    include/dxMathUtil.hpp
)

add_library(common::gfx ALIAS gfx)

auto_configure(gfx PRIVATE)

target_compile_definitions(gfx
    PRIVATE
        NOMINMAX
)

target_include_directories(gfx
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(gfx
    PRIVATE
        DirectX::d3dx12
        d3d12.lib
        dxguid.lib
        dxgi.lib
        D3DCompiler.lib
        assimp
        ddsLoader
        wicLoader
        common::mu
        common::coro
        common::mem
)

target_include_directories(gfx
    PUBLIC
        "${ASSIMP_INCLUDE}"
        "${TEXLOADER_INCLUDE}"
        "${D3D12_INCLUDE}"
)

# === net ===
add_library(net
    src/net.cpp include/net.hpp
)

add_library(common::net ALIAS net)

auto_configure(net PRIVATE)

target_link_libraries(net
    PRIVATE
        common::woon2exception
        ws2_32.lib
)

target_include_directories(net
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

include(configure_paths)
gen_resource_path()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.hpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/include/config.hpp")