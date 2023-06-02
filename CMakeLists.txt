cmake_minimum_required(VERSION 3.14)

#
# Project Configuration
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/ProjectDetails.cmake)
project(uvgrtp
        VERSION ${uvgrtp_VER}
        DESCRIPTION ${uvgrtp_DESCR}
        HOMEPAGE_URL ${uvgrtp_URL}
        LANGUAGES CXX)

include(GNUInstallDirs)


option(UVGRTP_DISABLE_CRYPTO "Do not build uvgRTP with crypto enabled" OFF)
option(UVGRTP_DISABLE_PRINTS "Do not print anything from uvgRTP" OFF)
option(UVGRTP_DISABLE_WERROR "Ignore compiler warnings" ON)

option(UVGRTP_DISABLE_TESTS    "Do not build unit tests" OFF)
option(UVGRTP_DISABLE_EXAMPLES "Do not build examples" OFF)
option(UVGRTP_DISABLE_INSTALL  "Do not install anything from uvgRTP" OFF)

option(UVGRTP_DOWNLOAD_CRYPTO  "Download headers for Crypto++ if they are missing" OFF)

option(UVGRTP_RELEASE_COMMIT "Explicitly say that this is a release version in version prints" OFF)

# obsolete, do not use
option(DISABLE_CRYPTO "Do not build uvgRTP with crypto enabled" OFF)
option(DISABLE_PRINTS "Do not print anything from uvgRTP" OFF)
option(DISABLE_WERROR "Ignore compiler warnings" OFF)

# offer some backwards compatibility for old flags
if (DISABLE_CRYPTO)
    set(UVGRTP_DISABLE_CRYPTO ON)
endif()
if (DISABLE_PRINTS)
    set(UVGRTP_DISABLE_PRINTS ON)
endif()
if (DISABLE_WERROR)
    set(UVGRTP_DISABLE_WERROR ON)
endif()


include(cmake/FindDependencies.cmake)
include(cmake/Versioning.cmake)


add_library(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES
        SOVERSION "${PROJECT_VERSION_MAJOR}"
        VERSION "${LIBRARY_VERSION}"
        )
set(UVGRTP_CXX_FLAGS "")
set(UVGRTP_LINKER_FLAGS "")

target_sources(${PROJECT_NAME} PRIVATE
        src/clock.cc
        src/crypto.cc
        src/frame.cc
        src/hostname.cc
        src/context.cc
        src/media_stream.cc
        src/mingw_inet.cc
        src/reception_flow.cc
        src/poll.cc
        src/frame_queue.cc
        src/random.cc
        src/rtcp.cc
        src/rtcp_packets.cc
        src/rtp.cc
        src/session.cc
        src/socket.cc
        src/zrtp.cc
        src/holepuncher.cc

        src/formats/media.cc
        src/formats/h26x.cc
        src/formats/h264.cc
        src/formats/h265.cc
        src/formats/h266.cc

        src/zrtp/zrtp_receiver.cc
        src/zrtp/hello.cc
        src/zrtp/hello_ack.cc
        src/zrtp/commit.cc
        src/zrtp/dh_kxchng.cc
        src/zrtp/confirm.cc
        src/zrtp/confack.cc
        src/zrtp/error.cc
        src/zrtp/zrtp_message.cc
        src/srtp/base.cc
        src/srtp/srtp.cc
        src/srtp/srtcp.cc
        src/wrapper_c.cc
        src/socketfactory.cc
        src/rtcp_reader.cc
        )

source_group(src/srtp src/srtp/.*)
source_group(src/formats src/formats/.*)
source_group(src/zrtp src/zrtp/.*)

source_group(src src/.*)
source_group(include include/.*)

# Including header files so VisualStudio will list them correctly
target_sources(${PROJECT_NAME} PRIVATE
        src/crypto.hh
        src/debug.hh
        src/global.hh
        src/random.hh
        src/holepuncher.hh
        src/hostname.hh
        src/mingw_inet.hh
        src/reception_flow.hh
        src/poll.hh
        src/rtp.hh
        src/rtcp_packets.hh
        src/socket.hh
        src/zrtp.hh
        src/frame_queue.hh
        src/memory.hh

        src/formats/h26x.hh
        src/formats/h264.hh
        src/formats/h265.hh
        src/formats/h266.hh
        src/formats/media.hh

        src/srtp/base.hh
        src/srtp/srtcp.hh
        src/srtp/srtp.hh

        src/zrtp/zrtp_receiver.hh
        src/zrtp/hello.hh
        src/zrtp/hello_ack.hh
        src/zrtp/commit.hh
        src/zrtp/dh_kxchng.hh
        src/zrtp/defines.hh
        src/zrtp/confirm.hh
        src/zrtp/confack.hh
        src/zrtp/error.hh
        src/zrtp/zrtp_message.hh
        src/srtp/base.hh
        src/srtp/srtp.hh
        src/srtp/srtcp.hh
        src/socketfactory.hh
        src/rtcp_reader.hh

        include/uvgrtp/util.hh
        include/uvgrtp/clock.hh
        include/uvgrtp/context.hh

        include/uvgrtp/frame.hh
        include/uvgrtp/lib.hh
        include/uvgrtp/media_stream.hh
        include/uvgrtp/rtcp.hh
        include/uvgrtp/session.hh
        include/uvgrtp/version.hh

        include/uvgrtp/wrapper_c.hh
        )

if(WIN32)
    set(WINLIBS wsock32 ws2_32)
endif()

target_link_libraries(${PROJECT_NAME}
        PRIVATE
            Threads::Threads
            ${PROJECT_NAME}_version
            ${WINLIBS}
        )

target_include_directories(${PROJECT_NAME}
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        PUBLIC $<INSTALL_INTERFACE:include>
        )

if(CMAKE_BUILD_TYPE)
    message(STATUS "Selecting build type: ${CMAKE_BUILD_TYPE}")
elseif (NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type selected. Selecting Release. You may use CMAKE_BUILD_TYPE to select a different build type.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /Zc:__cplusplus /W4)
else()
    if (UVGRTP_DISABLE_WERROR)
        target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    else ()
        target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()

if (UVGRTP_DISABLE_CRYPTO)
    list(APPEND UVGRTP_CXX_FLAGS "-D__RTP_NO_CRYPTO__")
    target_compile_definitions(${PROJECT_NAME} PRIVATE __RTP_NO_CRYPTO__)
endif()

if (UVGRTP_DISABLE_PRINTS)
    list(APPEND UVGRTP_CXX_FLAGS "-D__RTP_SILENT__")
    target_compile_definitions(${PROJECT_NAME} PRIVATE __RTP_SILENT__)
endif()

if (UNIX)
    # Check if platform-specific functions exist
    include(CheckCXXSymbolExists)
    check_cxx_symbol_exists(getrandom sys/random.h HAVE_GETRANDOM)
    check_cxx_symbol_exists(sendmsg sys/socket.h HAVE_SENDMSG)
    check_cxx_symbol_exists(sendmmsg sys/socket.h HAVE_SENDMMSG)
    if(HAVE_GETRANDOM)
        list(APPEND UVGRTP_CXX_FLAGS "-DUVGRTP_HAVE_GETRANDOM=1")
        target_compile_definitions(${PROJECT_NAME} PRIVATE UVGRTP_HAVE_GETRANDOM=1)
    endif()
    if(HAVE_SENDMSG)
        list(APPEND UVGRTP_CXX_FLAGS "-DUVGRTP_HAVE_SENDMSG=1")
        target_compile_definitions(${PROJECT_NAME} PRIVATE UVGRTP_HAVE_SENDMSG=1)
    endif()
    if(HAVE_SENDMMSG)
        list(APPEND UVGRTP_CXX_FLAGS "-DUVGRTP_HAVE_SENDMMSG=1")
        target_compile_definitions(${PROJECT_NAME} PRIVATE UVGRTP_HAVE_SENDMMSG=1)
    endif()

    # Try finding if pkg-config installed in the system
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        list(APPEND UVGRTP_LINKER_FLAGS "-luvgrtp")
        if(CMAKE_USE_PTHREADS_INIT AND NOT CMAKE_HAVE_LIBC_PTHREAD)
            list(APPEND UVGRTP_LINKER_FLAGS "-lpthread")
        endif()
        # Check PKG_CONFIG_PATH, if not defined, use lib/pkgconfig
        if(NOT DEFINED ENV{PKG_CONFIG_PATH})
            set(PKG_CONFIG_PATH "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
            message("PKG_CONFIG_PATH is not set. Setting it to ${PKG_CONFIG_PATH}")
        endif(NOT DEFINED ENV{PKG_CONFIG_PATH})

        # Find crypto++
        if(NOT UVGRTP_DISABLE_CRYPTO)
            pkg_search_module(CRYPTOPP libcrypto++ cryptopp)
            if(CRYPTOPP_FOUND)
              list(APPEND UVGRTP_CXX_FLAGS ${CRYPTOPP_CFLAGS_OTHER})
              list(APPEND UVGRTP_LINKER_FLAGS ${CRYPTOPP_LDFLAGS})
            endif()
        endif()

        # Generate and install .pc file
        string(REPLACE ";" " " UVGRTP_CXX_FLAGS "${UVGRTP_CXX_FLAGS}")
        string(REPLACE ";" " " UVGRTP_LINKER_FLAGS "${UVGRTP_LINKER_FLAGS}")
        configure_file("cmake/uvgrtp.pc.in" "uvgrtp.pc" @ONLY)
        if (NOT UVGRTP_DISABLE_INSTALL)
            install(FILES ${CMAKE_CURRENT_BINARY_DIR}/uvgrtp.pc DESTINATION ${PKG_CONFIG_PATH}/)
        endif()
    else()
        message("pkg-config not found. Not generating pc file")
    endif(PkgConfig_FOUND)
endif (UNIX)

if (NOT CRYPTOPP_FOUND AND UVGRTP_DOWNLOAD_CRYPTO)
  include(cmake/CryptoHeaders.cmake)
  list(APPEND UVGRTP_CXX_FLAGS ${CRYPTOPP_CFLAGS_OTHER})
  list(APPEND UVGRTP_LINKER_FLAGS ${CRYPTOPP_LDFLAGS})
elseif(NOT CRYPTOPP_FOUND AND UNIX)
  # In Visual Studio, we want to leave user the option to add Crypto++ headers manually
  message("libcrypto++ not found. Encryption will be disabled")
  list(APPEND UVGRTP_CXX_FLAGS "-D__RTP_NO_CRYPTO__")
endif()

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework Security")
endif()

if (NOT UVGRTP_DISABLE_EXAMPLES)
    add_subdirectory(examples EXCLUDE_FROM_ALL)
endif()

if (NOT UVGRTP_DISABLE_TESTS)
    add_subdirectory(test EXCLUDE_FROM_ALL)
endif()

if (NOT UVGRTP_DISABLE_INSTALL)
    # Install
    #
    # Define install target, install libraries and archives (static libraries) to "<prefix>/..."
    install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}_version EXPORT ${PROJECT_NAME}Targets
            LIBRARY
                DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT ${PROJECT_NAME}_Runtime
            ARCHIVE
                DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT ${PROJECT_NAME}_Runtime
            RUNTIME
                DESTINATION ${CMAKE_INSTALL_BINDIR}
                COMPONENT ${PROJECT_NAME}_Runtime)

    #Copy all header files to the <prefix>/include/uvgrtp directory
    file(GLOB DEPLOY_FILES_AND_DIRS "${CMAKE_SOURCE_DIR}/include/uvgrtp/*")
    install(FILES ${DEPLOY_FILES_AND_DIRS}
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/
            COMPONENT ${PROJECT_NAME}_Develop)

    #Create a File representing the current library version
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}ConfigVersion.cmake"
            COMPATIBILITY SameMajorVersion
    )

    #Create a Targets file representing exported targets (for usage within the build tree)
    export(EXPORT ${PROJECT_NAME}Targets
            FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Targets.cmake"
            NAMESPACE ${PROJECT_NAME}::
            )

    #Copy "cmake/uvgrtpConfig.cmake" to "${CMAKE_CURRENT_BINARY_DIR}/uvgrtp/uvgrtpConfig.cmake"
    configure_file(cmake/${PROJECT_NAME}Config.cmake
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Config.cmake"
            COPYONLY
            )

    #Copy "cmake/uvgrtpMacros.cmake" to "${CMAKE_CURRENT_BINARY_DIR}/uvgrtp/uvgrtpMacros.cmake"
    configure_file(cmake/${PROJECT_NAME}Macros.cmake
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Macros.cmake"
            COPYONLY
            )

    #
    # Adding target to installing cmake package
    #
    set(ConfigPackageLocation lib/cmake/${PROJECT_NAME})
    install(EXPORT ${PROJECT_NAME}Targets
            FILE ${PROJECT_NAME}Targets.cmake
            NAMESPACE ${PROJECT_NAME}::
            DESTINATION ${ConfigPackageLocation}
            )

    install(FILES cmake/${PROJECT_NAME}Config.cmake cmake/${PROJECT_NAME}Macros.cmake
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}ConfigVersion.cmake"
            DESTINATION ${ConfigPackageLocation}
            COMPONENT uvgRTPMain
    )

    # Packaging
    #
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        add_subdirectory(packaging)
    endif()
endif()