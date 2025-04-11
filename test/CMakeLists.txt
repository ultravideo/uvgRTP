find_package(Git)

if (Git_FOUND)

    project(uvgrtp_test)

    enable_testing()
    include(GoogleTest)

    add_executable(${PROJECT_NAME})
    target_sources(${PROJECT_NAME} PRIVATE
                main.cpp
                test_1_version.cpp
                test_2_rtp.cpp
                test_3_rtcp.cpp
                test_4_formats.cpp
                test_5_srtp_zrtp.cpp
                test_6_scl_unit_test.cpp
                test_common.hh
            )

    target_include_directories(${PROJECT_NAME} PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../src>)

    # set crypto++ to be linked in tests if available
    if (NOT UVGRTP_DISABLE_CRYPTO AND CRYPTOPP_FOUND)
        if(MSVC)
            set(CRYPTOPP_LIB_NAME "cryptlib")
        else()
            set(CRYPTOPP_LIB_NAME "cryptopp")
        endif()
    else()
        set(CRYPTOPP_LIB_NAME "")
    endif()

    target_link_libraries(${PROJECT_NAME} PRIVATE GTest::GTestMain uvgrtp ${CRYPTOPP_LIB_NAME})
    
    # make sure the test binary can access the shared version of library
    if(UVGRTP_BUILD_SHARED)
        if(APPLE)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E env DYLD_LIBRARY_PATH=$<TARGET_FILE_DIR:uvgrtp>:$ENV{DYLD_LIBRARY_PATH} $<TARGET_FILE:${PROJECT_NAME}>)
        elseif(UNIX)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=$<TARGET_FILE_DIR:uvgrtp>:$ENV{LD_LIBRARY_PATH} $<TARGET_FILE:${PROJECT_NAME}>)
        else()
            # Copy the shared library for Windows (if needed)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:uvgrtp>
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>)
        endif()
    endif()

    gtest_add_tests(TARGET ${PROJECT_NAME})
else()
    message(WARNING "Git not found, not building tests")
endif()