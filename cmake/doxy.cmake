macro(doxygen_option OPTION_NAME DESCRIPTION DEFAULT)
    message(STATUS "${${OPTION_NAME}_CONFIG}")
    option("${OPTION_NAME}_CONFIG" "${DESCRIPTION}" ${DEFAULT})
    if(${OPTION_NAME}_CONFIG)
        message(STATUS "Doxygen option ${OPTION_NAME} is enabled")
        set("${OPTION_NAME}" YES)
    else()
        message(STATUS "Doxygen option ${OPTION_NAME} is disabled")
        set("${OPTION_NAME}" NO)
    endif()
endmacro()

function(add_doxygen DOXYGEN_INPUT_DIR DOXYGEN_OUTPUT_DIR)
    find_package(Doxygen)
    if(NOT DOXYGEN_FOUND)
        return()
    endif()

    set(Doxygen_ROOT "" CACHE PATH
        "The path to directory where doxygen is installed
        used in find_package(Doxygen),
        This path is used prior to any other paths."
    )

    doxygen_option(DOXYGEN_RECURSIVE "doxygen recursive option" OFF)

    doxygen_option(DOXYGEN_EXTRACT_ALL
        "extract all entities including even enitites not documented.
        (turn this option on when you need develop-documentation.)"
        OFF
    )

    option(BUILD_DOXYGEN
        "enable if you want to document with doxygen 
        each time you build the project."
        OFF
    )

    if(BUILD_DOXYGEN)
        set(DOXYGEN_ALL ALL)
    endif()

    configure_file("${DOXYGEN_INPUT_DIR}/Doxyfile.in"
        "${DOXYGEN_OUTPUT_DIR}/Doxyfile"
        @ONLY
    )

    doxygen_add_docs(doxygen
        "${DOXYGEN_INPUT_DIR}"
        WORKING_DIRECTORY "${DOXYGEN_INPUT_DIR}"
        ${DOXYGEN_ALL}
        COMMENT "custom target for automatic documentation"
        CONFIG_FILE "${DOXYGEN_OUTPUT_DIR}/Doxyfile"
    )
endfunction()