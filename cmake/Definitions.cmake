set(UVGRTP_DEFS_FILE "${CMAKE_CURRENT_BINARY_DIR}/uvgrtp_defs.hh")

file(WRITE ${UVGRTP_DEFS_FILE} 
    "#pragma once\n"
)

if (UVGRTP_BUILD_SHARED)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UVGRTP_SHARED)
    file(APPEND ${UVGRTP_DEFS_FILE} "#define UVGRTP_SHARED_API 1\n")
else()
    file(APPEND ${UVGRTP_DEFS_FILE} "#define UVGRTP_SHARED_API 0\n")
endif()

target_sources(${PROJECT_NAME} PRIVATE ${UVGRTP_DEFS_FILE})

# Install the generated file so external projects can use it
install(FILES ${UVGRTP_DEFS_FILE} DESTINATION include)