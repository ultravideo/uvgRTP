project(tests)
cmake_minimum_required(VERSION 3.12)

add_executable(configuration configuration.cc)
set_target_properties(configuration PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
target_link_libraries(configuration pthread)
target_link_libraries(configuration uvgrtp)

if (UNIX)
    set(COMPILER_FLAGS "-Wall -Wextra -Wuninitialized -Wshadow -O2 -g -march=native -DNDEBUG -mno-avx512f")
    if (DISABLE_CRYPTO)
        string(APPEND COMPILER_FLAGS " -D__RTP_NO_CRYPTO__")
    else(DISABLE_CRYPTO)
        target_link_libraries(configuration crypto++)
    endif(DISABLE_CRYPTO)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILER_FLAGS}")
