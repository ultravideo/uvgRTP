include(GenerateExportHeader)

# Generate the actual export definitions
generate_export_header(${PROJECT_NAME} 
    BASE_NAME UVGRTP
    EXPORT_MACRO_NAME UVGRTP_EXPORT
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/uvgrtp/export.hh"
)

target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include/uvgrtp/export.hh)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/uvgrtp/export.hh" DESTINATION include)