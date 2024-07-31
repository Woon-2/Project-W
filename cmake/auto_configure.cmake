function(auto_configure TARGET_NAME ACCESS_MODIFIER)
    target_compile_features("${TARGET_NAME}" ${ACCESS_MODIFIER} cxx_std_20)
    
    # See below link for msvc options
    # https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options?view=msvc-170
    if(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
        target_compile_options("${TARGET_NAME}"
            ${ACCESS_MODIFIER}
                /MP
                $<IF:$<CONFIG:Debug>,/Od,/O2>
                /W3
                /Zc:preprocessor
                /Zc:__cplusplus
                /sdl-
                /fp:fast
                /JMC
        )
    # See below link for gcc options
    # https://gcc.gnu.org/onlinedocs/gcc/Option-Summary.html
    elseif(CMAKE_CXX_COMPILER_ID MATCHES GNU)
        target_compile_options("${TARGET_NAME}"
            ${ACCESS_MODIFIER}
                -Wall
                -Wextra
                -pedantic
                -fconcepts
                $<IF:$<CONFIG:Debug>,-O0,-O2>
        )
    # See below link for clang options
    # https://clang.llvm.org/docs/ClangCommandLineReference.html
    elseif(CMAKE_CXX_COMPILER_ID MATCHES CLANG)
        # Clang options differ by platform
        if(WIN32)   # clang-cl
            target_compile_options("${TARGET_NAME}"
                ${ACCESS_MODIFIER}
                    /clang:-fcoroutines-ts
                    -fms-compatibility
            )
        else()  # AppleClang or Clang on Linux
            target_compile_options("${TARGET_NAME}"
                ${ACCESS_MODIFIER}
                    -std=c++2a
            )
        endif()
    
    else()
        message(WARNING "building with unknown compiler,
            the build might not be successful."
        )
    endif()

    # Platform setting
    if(WIN32)
        set_target_properties("${TARGET_NAME}"
            PROPERTIES
                WINDOWS_EXPORT_ALL_SYMBOLS OFF
        )
    endif()

    target_compile_definitions("${TARGET_NAME}"
        ${ACCESS_MODIFIER}
            $<$<CONFIG:Debug>:DEBUG>
            $<$<CONFIG:Release>:NDEBUG>
    )

    target_compile_definitions("${TARGET_NAME}"
        ${ACCESS_MODIFIER}
            
    )

    target_compile_options("${TARGET_NAME}"
    ${ACCESS_MODIFIER}
        $<IF:$<BOOL:${BUILD_SHARED_LIBS}>,
            $<IF:$<CONFIG:Release>,/MD,/MDd>, # Multi-threaded & Dynamic libraries
            $<IF:$<CONFIG:Release>,/MT,/MTd> # Multi-threaded & Static libraries
        >
    )

    target_compile_definitions("${TARGET_NAME}"
        ${ACCESS_MODIFIER}
            NOEXCEPT=noexcept
    )

endfunction()