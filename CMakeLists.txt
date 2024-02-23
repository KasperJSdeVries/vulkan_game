cmake_minimum_required(VERSION 3.24)
project(game LANGUAGES C VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

find_package(glfw3 3.3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(cglm REQUIRED)

file(GLOB_RECURSE SOURCES "src/*.c")

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} glfw ${Vulkan_LIBRARY} cglm)
target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIR})

install(TARGETS ${PROJECT_NAME})
