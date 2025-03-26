set(UVGRTP_DEFS_FILE "${CMAKE_CURRENT_BINARY_DIR}/include/uvgrtp_defs.hh")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include")
file(WRITE ${UVGRTP_DEFS_FILE} 
    "#pragma once\n\n"
)

if (UVGRTP_BUILD_SHARED)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UVGRTP_SHARED)
    file(APPEND ${UVGRTP_DEFS_FILE} "#define UVGRTP_EXTENDED_API 0\n")
else()
    file(APPEND ${UVGRTP_DEFS_FILE} "#define UVGRTP_EXTENDED_API 1\n")
endif()


target_sources(${PROJECT_NAME} PRIVATE ${UVGRTP_DEFS_FILE})

# Install the generated file so external projects can use it
install(FILES ${UVGRTP_DEFS_FILE} DESTINATION include)