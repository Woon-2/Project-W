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
include(win32app_configuration)

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

# === gfx ===
add_library(gfx
    src/gfx.cpp include/gfx.hpp
    src/gfxExcept.cpp include/gfxExcept.hpp
    src/dxfactory.cpp include/dxfactory.hpp
    src/dxexcept.cpp include/dxexcept.hpp
    src/d3d12core.cpp include/d3d12core.hpp
    src/d3dwindow.cpp include/d3dwindow.hpp
    src/sampleRenderer.cpp include/sampleRenderer.hpp
    src/sampleScene.cpp include/sampleScene.hpp
    src/d3d12shader.cpp include/d3d12shader.hpp
    src/rootPresets.cpp include/rootPresets.hpp
    src/d3d12res.cpp include/d3d12res.hpp
    src/vertex.cpp include/vertex.hpp
    src/mesh.cpp include/mesh.hpp
    src/d3d12mesh.cpp include/d3d12mesh.hpp
    src/inputLayout.cpp include/inputLayout.hpp
    src/d3d12InputLayout.cpp include/d3d12InputLayout.hpp
    src/nullShader.cpp include/nullShader.hpp
    src/inputLayoutPresets.cpp include/inputLayoutPresets.hpp
    src/d3d12InputLayoutPresets.cpp include/d3d12InputLayoutPresets.hpp

    include/dxtarget.hpp
    include/gfxPrimitive.hpp
    include/shaderPath.hpp
    include/enumUtil.hpp
)

add_library(common::gfx ALIAS gfx)

auto_configure(gfx PRIVATE)

target_include_directories(gfx
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(gfx
    PRIVATE
        d3d12.lib
        dxguid.lib
        dxgi.lib
        D3DCompiler.lib
        assimp
)

target_include_directories(gfx
    PUBLIC
        "${ASSIMP_INCLUDE}"
)

include(configure_paths)
gen_resource_path()