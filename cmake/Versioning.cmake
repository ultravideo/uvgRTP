if(GIT_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            RESULT_VARIABLE result
            OUTPUT_VARIABLE uvgrtp_GIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(result)
        message(WARNING "Failed to get git hash: ${result}")
    endif()
endif()

if(uvgrtp_GIT_HASH)
    SET(uvgrtp_GIT_HASH "${uvgrtp_GIT_HASH}")
endif()

option(RELEASE_COMMIT "Create a release version" OFF)
if(RELEASE_COMMIT)
    set (LIBRARY_VERSION ${PROJECT_VERSION})
else()
    set (LIBRARY_VERSION ${PROJECT_VERSION} + "-" + ${uvgrtp_GIT_HASH})
endif()

configure_file(cmake/version.cpp.in version.cpp
        @ONLY
        )
add_library(${PROJECT_NAME}_version OBJECT
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)
target_include_directories(${PROJECT_NAME}_version
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

if (RELEASE_COMMIT)
    target_compile_definitions(${PROJECT_NAME}_version PRIVATE RTP_RELEASE_COMMIT)
endif()

