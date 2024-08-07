cmake_minimum_required(VERSION 3.24)
project(
  game
  LANGUAGES C
  VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Wextra -Wpedantic -Wformat")

find_package(glfw3 3.3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(cglm REQUIRED)

find_program(GLSL_VALIDATOR glslangValidator)
if(NOT GLSL_VALIDATOR)
  message(FATAL "Could not find glslangValidator compiler")
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES "res/shaders/*.vert" "res/shaders/*.frag")

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
    COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.3 ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})

set(SOURCES
    src/camera.c
    src/command_buffer.c
    src/context.c
    src/darray.c
    src/device.c
    src/font.c
    src/gltf.c
    src/json.c
    src/main.c
    src/pipeline.c
    src/swapchain.c)

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} glfw ${Vulkan_LIBRARY} cglm m)
target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIR}
                                                   ${STB_INCLUDE_PATH})

add_dependencies(${PROJECT_NAME} Shaders)

add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_BINARY_DIR}/shaders"
          "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders")

add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "$<TARGET_FILE_DIR:${PROJECT_NAME}>/models"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/res/models"
          "$<TARGET_FILE_DIR:${PROJECT_NAME}>/models")

add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "$<TARGET_FILE_DIR:${PROJECT_NAME}>/fonts"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/res/fonts"
          "$<TARGET_FILE_DIR:${PROJECT_NAME}>/fonts")

add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "$<TARGET_FILE_DIR:${PROJECT_NAME}>/textures"
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/res/textures"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/textures")

install(TARGETS ${PROJECT_NAME})
install(DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders" TYPE DATA)
install(DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>/textures" TYPE DATA)
install(DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>/models" TYPE DATA)
install(DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>/fonts" TYPE DATA)
