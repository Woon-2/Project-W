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
    src/FSM.cpp include/FSM.hpp

    include/generator.hpp
    include/resumable.hpp
)

add_library(common::coro ALIAS coro)

auto_configure(coro PUBLIC)

target_include_directories(coro
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# === TMP ===
add_library(TMP
    INTERFACE
        include/TMP.hpp
)

add_library(common::TMP ALIAS TMP)

auto_configure(TMP INTERFACE)

target_include_directories(TMP
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
    src/memUtil.cpp include/memUtil.hpp
)

add_library(common::mem ALIAS mem)

auto_configure(mem PRIVATE)

target_include_directories(mem
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# === ecs ===
add_library(ecsClient
    src/game/level.cpp include/game/level.hpp
    src/coord.cpp include/coord.hpp
    src/game/physicsSystem.cpp include/game/physicsSystem.hpp
    src/game/animSystem.cpp include/game/animSystem.hpp

    src/ecs.cpp include/ecs.hpp
    include/ecsExcept.hpp
    include/enumUtil.hpp
    include/TMP.hpp
)

add_library(common::ecsClient ALIAS ecsClient)

target_compile_definitions(ecsClient
    PUBLIC
        ECS_CLIENT
)

auto_configure(ecsClient PRIVATE)

target_link_libraries(ecsClient
    PRIVATE
        common::mu
        common::TMP
        common::coro
)

target_include_directories(ecsClient
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_library(ecsServer
    src/game/level.cpp include/game/level.hpp
    src/coord.cpp include/coord.hpp
    src/game/physicsSystem.cpp include/game/physicsSystem.hpp

    src/ecs.cpp include/ecs.hpp
    include/ecsExcept.hpp
    include/enumUtil.hpp
    include/TMP.hpp
)

add_library(common::ecsServer ALIAS ecsServer)

target_compile_definitions(ecsServer
    PUBLIC
        ECS_SERVER
)

auto_configure(ecsServer PRIVATE)

target_link_libraries(ecsServer
    PRIVATE
        common::mu
        common::woon2exception
        common::TMP
        common::coro
)

target_include_directories(ecsServer
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# === net ===
add_library(netClient
    src/net/session.cpp include/net/session.hpp
    src/net/netLow.cpp include/net/netLow.hpp

    include/net/protocol.hpp
    include/net/netInclude.hpp
)

add_library(common::netClient ALIAS netClient)

auto_configure(netClient PRIVATE)
define_win32_lighten_macros(netClient PUBLIC)

target_link_libraries(netClient
    PRIVATE
        common::mu
        common::ecsClient
        common::woon2exception
        ws2_32.lib
)

target_include_directories(netClient
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_library(netServer
    src/net/session.cpp include/net/session.hpp
    src/net/netLow.cpp include/net/netLow.hpp

    include/net/protocol.hpp
    include/net/netInclude.hpp
)

add_library(common::netServer ALIAS netServer)

auto_configure(netServer PRIVATE)
define_win32_lighten_macros(netServer PUBLIC)

target_link_libraries(netServer
    PRIVATE
        common::mu
        common::ecsServer
        common::woon2exception
        ws2_32.lib
)

target_include_directories(netServer
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# === gfx ===
add_library(gfx
    src/gfxExcept.cpp include/gfxExcept.hpp
    src/dxutil/dxexcept.cpp include/dxutil/dxexcept.hpp
    src/dxutil/dxLow.cpp include/dxutil/dxLow.hpp
    src/vertex.cpp include/vertex.hpp
    src/d3d12util/d3d12Low.cpp include/d3d12util/d3d12Low.hpp
    src/d3d12util/d3d12ResourceXX.cpp include/d3d12util/d3d12ResourceXX.hpp
    src/d3d12util/d3d12ShaderXX.cpp include/d3d12util/d3d12ShaderXX.hpp
    src/d3d12util/d3d12RenderPass.cpp include/d3d12util/d3d12RenderPass.hpp
    src/d3d12engine/d3d12Engine.cpp include/d3d12engine/d3d12Engine.hpp
    src/game/animSystem.cpp include/game/animSystem.hpp
    
    include/shaderPath.hpp
    include/enumUtil.hpp
    include/d3d12engine/descriptorRangeSpec.hpp
    include/TMP.hpp
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
        freeImage
        common::mu
        common::coro
        common::mem
        common::TMP
        common::ecsClient
)

target_include_directories(gfx
    PUBLIC
        "${ASSIMP_INCLUDE}"
        "${TEXLOADER_INCLUDE}"
        "${D3D12_INCLUDE}"
        "${FREEIMAGE_INCLUDE}"
)

# === shaders ===
include(find_hlsl_compiler)

set(SHADER_PATH ${CMAKE_CURRENT_SOURCE_DIR}/shaders CACHE PATH
    "Write path to populate shaders."
)

set(COMPILED_SHADER_PATH "${CMAKE_CURRENT_BINARY_DIR}/shaders" CACHE PATH
    "Write path to populate compiled shaders."
)

compile_hlsl("${SHADER_PATH}/pbrShader.hlsl"
    "${COMPILED_SHADER_PATH}/pbrShader_vs.cso"
    "vs_5_1"
    "VSMain"
    /enable_unbounded_descriptor_tables
)

compile_hlsl("${SHADER_PATH}/pbrShader.hlsl"
    "${COMPILED_SHADER_PATH}/pbrShader_ps.cso"
    "ps_5_1"
    "PSMain"
    /enable_unbounded_descriptor_tables
)

add_hlsl_target("${HLSL_COMPILED_SHADERS}")

include(configure_paths)
gen_shader_path()

add_dependencies(gfx hlsl)


gen_resource_path()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.hpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/include/config.hpp")