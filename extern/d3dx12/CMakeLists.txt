cmake_minimum_required(VERSION 3.27)

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

project(PD3DX12 VERSION 2024.09.08 LANGUAGES CXX)

message(STATUS "Configuring ${CMAKE_PROJECT_NAME}(v${CMAKE_PROJECT_VERSION})...")

# Add its own cmake directory to the module path
# When seperating this project from the main project,
# having the cmake directory contain auto_configure.cmake and win32_configuration.cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

add_library(d3dx12
    src/d3dx12_property_format_table.cpp
    src/dxguids.cpp
)

add_library(DirectX::d3dx12 ALIAS d3dx12)

include(auto_configure)
include(win32_configuration)

auto_configure(d3dx12 PRIVATE)
define_win32_lighten_macros(d3dx12 PRIVATE)

target_include_directories(d3dx12
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

set(D3D12_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include"
    CACHE PATH "Path to DirectX-Headers include files"
)