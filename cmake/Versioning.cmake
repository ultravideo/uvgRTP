if(GIT_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE result
            OUTPUT_VARIABLE uvgrtp_GIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(result)
        message(STATUS "Failed to get git hash")
    else()
        message(STATUS "Got git hash: ${uvgrtp_GIT_HASH}")
    endif()
endif()

if(uvgrtp_GIT_HASH)
    SET(uvgrtp_GIT_HASH "${uvgrtp_GIT_HASH}")
endif()

if(UVGRTP_RELEASE_COMMIT)
    set (LIBRARY_VERSION ${PROJECT_VERSION})
elseif(uvgrtp_GIT_HASH)
    set (LIBRARY_VERSION ${PROJECT_VERSION} + "-" + ${uvgrtp_GIT_HASH})
else()
    set (LIBRARY_VERSION ${PROJECT_VERSION} + "-source")
    set(uvgrtp_GIT_HASH "source")
endif()

configure_file(cmake/version.cc.in version.cc
        @ONLY
        )
add_library(${PROJECT_NAME}_version OBJECT
        ${CMAKE_CURRENT_BINARY_DIR}/version.cc)
target_include_directories(${PROJECT_NAME}_version
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

if (UVGRTP_RELEASE_COMMIT)
    target_compile_definitions(${PROJECT_NAME}_version PRIVATE RTP_RELEASE_COMMIT)
endif()

