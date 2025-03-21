set(UVGRTP_DEFS_FILE "${CMAKE_CURRENT_BINARY_DIR}/uvgrtp_defs.hh")

file(WRITE ${UVGRTP_DEFS_FILE} 
    "#pragma once\n\n"
)

if (UVGRTP_BUILD_SHARED)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UVGRTP_SHARED)
    file(APPEND ${UVGRTP_DEFS_FILE} "#define UVGRTP_EXTENDED_API 0\n")
else()
    file(APPEND ${UVGRTP_DEFS_FILE} "#define UVGRTP_EXTENDED_API 1\n")
endif()



file(APPEND ${UVGRTP_DEFS_FILE} "\n/**\n")
file(APPEND ${UVGRTP_DEFS_FILE} " * \\defgroup CORE_API Core API\n")
file(APPEND ${UVGRTP_DEFS_FILE} " * \\brief Available for both static and shared builds.\n")
file(APPEND ${UVGRTP_DEFS_FILE} " */\n\n")
file(APPEND ${UVGRTP_DEFS_FILE} "/**\n")
file(APPEND ${UVGRTP_DEFS_FILE} " * \\defgroup EXTENDED_API Extended C++ API\n")
file(APPEND ${UVGRTP_DEFS_FILE} " * \\brief Only available in static builds.\n")
file(APPEND ${UVGRTP_DEFS_FILE} " */\n\n")


target_sources(${PROJECT_NAME} PRIVATE ${UVGRTP_DEFS_FILE})

# Install the generated file so external projects can use it
install(FILES ${UVGRTP_DEFS_FILE} DESTINATION include)