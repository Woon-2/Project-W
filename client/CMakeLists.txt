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

add_executable(client)

target_sources(client
    PRIVATE
        src/main.cpp
        src/app.cpp include/app.hpp
        src/game.cpp include/game.hpp
        src/mygfx.cpp include/mygfx.hpp
        include/testInput.hpp
        src/ecs.cpp include/ecs.hpp
        src/ecsExcept.cpp include/ecsExcept.hpp
        src/player.cpp include/player.hpp
        src/inputSystem.cpp include/inputSystem.hpp
        
        src/Timer.cpp include/Timer.hpp
)

include(auto_configure)

auto_configure(client PRIVATE)

target_include_directories(client
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(client
    PRIVATE
        common::woon2exception
        common::win32
        common::gfx
        common::input
)

include(find_hlsl_compiler)

set(COMPILED_SHADER_PATH "${CMAKE_CURRENT_BINARY_DIR}/shader" CACHE PATH
    "Write path to populate compiled shaders."
)

compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/shader/nullShader.hlsl"
    "${COMPILED_SHADER_PATH}/nullShader_vs.cso"
    "vs_5_1"
    "VSMain"
)
compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/shader/nullShader.hlsl"
    "${COMPILED_SHADER_PATH}/nullShader_ps.cso"
    "ps_5_1"
    "PSMain"
)

compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/shader/solidShader.hlsl"
    "${COMPILED_SHADER_PATH}/solidShader_vs.cso"
    "vs_5_1"
    "VSMain"
)
compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/shader/solidShader.hlsl"
    "${COMPILED_SHADER_PATH}/solidShader_ps.cso"
    "ps_5_1"
    "PSMain"
)

compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/shader/ntShader.hlsl"
    "${COMPILED_SHADER_PATH}/ntShader_vs.cso"
    "vs_5_1"
    "VSMain"
    /D INCLUDE_drawcallIdxOnly
    /D INCLUDE_basicInstancing
    /D INCLUDE_phongLighting
)

compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/shader/ntShader.hlsl"
    "${COMPILED_SHADER_PATH}/ntShader_ps.cso"
    "ps_5_1"
    "PSMain"
    /D INCLUDE_drawcallIdxOnly
    /D INCLUDE_basicInstancing
    /D INCLUDE_phongLighting
)

compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/shader/tShader.hlsl"
    "${COMPILED_SHADER_PATH}/tShader_vs.cso"
    "vs_5_1"
    "VSMain"
    /D INCLUDE_drawcallIdxOnly
    /D INCLUDE_basicInstancing
    /D INCLUDE_phongLighting
)

compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/shader/tShader.hlsl"
    "${COMPILED_SHADER_PATH}/tShader_ps.cso"
    "ps_5_1"
    "PSMain"
    /D INCLUDE_drawcallIdxOnly
    /D INCLUDE_basicInstancing
    /D INCLUDE_phongLighting
)

add_hlsl_target("${HLSL_COMPILED_SHADERS}")

include(configure_paths)
gen_shader_path()

add_dependencies(client hlsl)

file(COPY "${PROJECT_SOURCE_DIR}/extern/assimp/assimp-vc143-mt.dll"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Debug"
)

file(COPY "${PROJECT_SOURCE_DIR}/extern/assimp/assimp-vc143-mt.dll"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Release"
)