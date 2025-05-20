cmake_minimum_required(VERSION 3.16)
project(chunk_map_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

add_executable(test_chunk_map
    test_chunk_map.cpp
    chunk_map.h
)

add_executable(test_flat_tree_map
    test_flat_tree_map.cpp
    flat_tree_map.h
    arrow_proxy.h
)

add_executable(test_flyweight_map
    test_flyweight_map.cpp
    flyweight_map.h
)
