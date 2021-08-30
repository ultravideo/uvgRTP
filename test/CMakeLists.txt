project(uvgrtp_test)

enable_testing()
include(GoogleTest)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
        PRIVATE
            main.cpp
            TestGTest.cpp
        )
target_link_libraries(${PROJECT_NAME}
        PRIVATE
            GTest::GTestMain
            uvgrtp
        )

gtest_add_tests(
        TARGET ${PROJECT_NAME}
)