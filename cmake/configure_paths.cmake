function(gen_shader_path)
    configure_file("${PROJECT_SOURCE_DIR}/common/shaderPath.hpp.in" "${PROJECT_SOURCE_DIR}/common/include/shaderPath.hpp" @ONLY)
endfunction()