add_library(assimp SHARED IMPORTED GLOBAL)

set_target_properties(assimp PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/assimp-vc143-mt.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
    IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/assimp-vc143-mt.lib"
)

set(ASSIMP_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include"
    CACHE PATH "spcify the path to assimp include directory"
)