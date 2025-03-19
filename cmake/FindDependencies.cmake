# PThread
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package( Threads REQUIRED )

if (NOT UVGRTP_DISABLE_TESTS)
    # Git
    find_package(Git)

    # GTest / GMock
    include(FetchContent)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        6910c9d9165801d8827d628cb72eb7ea9dd538c5 # v1.16.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)
    add_library(GTest::GMock ALIAS gmock)
    add_library(GTest::GMockMain ALIAS gmock_main)
    add_library(GTest::GTest ALIAS gtest)
    add_library(GTest::GTestMain ALIAS gtest_main)
endif()
