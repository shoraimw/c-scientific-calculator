cmake_minimum_required(VERSION 3.16)
project(c_scientific_calculator C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(raylib 5.0 REQUIRED)
add_executable(c_scientific_calculator main.c)
target_link_libraries(c_scientific_calculator PRIVATE raylib m)

if (WIN32)
    target_link_libraries(c_scientific_calculator PRIVATE winmm)
endif()
