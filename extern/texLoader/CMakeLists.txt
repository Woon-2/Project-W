set(TEXLOADER_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include"
    CACHE PATH "spcify the path to assimp include directory"
)

add_library(ddsLoader SHARED IMPORTED GLOBAL)

set_target_properties(ddsLoader PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/ddsLoader.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
    IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/ddsLoader.lib"
)

add_library(wicLoader SHARED IMPORTED GLOBAL)

set_target_properties(wicLoader PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/wicLoader.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
    IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/wicLoader.lib"
)