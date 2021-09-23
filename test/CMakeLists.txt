project(uvgrtp_test)

enable_testing()
include(GoogleTest)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
        PRIVATE
            main.cpp
            tests_encryption.cpp
            tests_formats.cpp
            tests_rtcp.cpp
            tests_rtp.cpp
            tests_version.cpp
        )

if(MSVC)
    target_link_libraries(${PROJECT_NAME}
            PRIVATE
                GTest::GTestMain
                uvgrtp
                cryptlib
            )
else()
    target_link_libraries(${PROJECT_NAME}
            PRIVATE
                GTest::GTestMain
                uvgrtp
                cryptopp
            )
endif()

gtest_add_tests(
        TARGET ${PROJECT_NAME}
)