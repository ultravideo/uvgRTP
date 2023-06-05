find_package(Git)

if (Git_FOUND)
    include(FetchContent)

    message(STATUS "Downloading Crypto++ headers")

    FetchContent_Declare(cryptopp
      URL https://github.com/weidai11/cryptopp/releases/download/CRYPTOPP_8_7_0/cryptopp870.zip
      URL_HASH SHA256=d0d3a28fcb5a1f6ed66b3adf57ecfaed234a7e194e42be465c2ba70c744538dd
      DOWNLOAD_EXTRACT_TIMESTAMP ON
    )

    FetchContent_MakeAvailable(cryptopp)

    file(GLOB CRYPTOPP_HEADERS "${cryptopp_SOURCE_DIR}/*.h")
    file(MAKE_DIRECTORY ${cryptopp_SOURCE_DIR}/include/cryptopp)
    file(COPY ${CRYPTOPP_HEADERS} DESTINATION ${cryptopp_SOURCE_DIR}/include/cryptopp)

    include_directories(${cryptopp_SOURCE_DIR}/include)
else()
    message(WARNING, "Git not found, not downloading Crypto++ headers!")
endif()