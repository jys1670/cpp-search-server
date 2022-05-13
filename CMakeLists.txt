cmake_minimum_required(VERSION 3.21)
project(search_server CXX)
set(CMAKE_CXX_STANDARD 17)

set(ENABLE_WARNINGS_AS_ERRORS false)
set(ENABLE_STDLIB_DEBUG_MACROS false)
set(ENABLE_SANITIZERS false)


# Multithreading
add_compile_options(-ltbb -lpthread)
add_link_options(-ltbb -lpthread)


set(CMAKE_CXX_FLAGS_DEBUG "-ggdb -O0 -fno-inline -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0")

if((ENABLE_WARNINGS_AS_ERRORS) AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
        message("WARNINGS as errors enabled")
        add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

if((ENABLE_STDLIB_DEBUG_MACROS) AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
        message("STDLIB debugging enabled")
        add_compile_definitions("_GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC")
endif()

if((ENABLE_SANITIZERS) AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
        message("SANITIZERS enabled")
        add_compile_options(
                -fsanitize=address
                #-fsanitize=thread
                -fsanitize=leak
                -fno-omit-frame-pointer
                -fno-optimize-sibling-calls
        )
        add_link_options(
                -fsanitize=address
                #-fsanitize=thread
                -fsanitize=leak
                -fno-omit-frame-pointer
                -fno-optimize-sibling-calls
        )
endif()


aux_source_directory(search-server SOURCES)
add_executable(cpp_search_server ${SOURCES})
