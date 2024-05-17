cmake_minimum_required(VERSION 3.12)
project(vm)

set(SOURCES src/vec3.c)

add_library(vm ${SOURCES})
target_include_directories(vm PUBLIC include)
target_link_libraries(vm PRIVATE m quadmath)
