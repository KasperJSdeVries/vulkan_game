cmake_minimum_required(VERSION 3.24)
project(
  game
  LANGUAGES C
  VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Wextra -Wpedantic")

find_package(Vulkan REQUIRED)

find_program(GLSL_VALIDATOR glslangValidator)
if(NOT GLSL_VALIDATOR)
  message(FATAL "Could not find glslangValidator compiler")
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES "res/shaders/*.vert" "res/shaders/*.frag")

foreach(glsl ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${glsl} NAME)
  set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
    COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.3 ${glsl} -o ${SPIRV}
    DEPENDS ${glsl}
    COMMENT "Compiled ${SPIRV}")
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(glsl)

add_custom_target(
  Shaders
  DEPENDS ${SPIRV_BINARY_FILES}
  COMMENT "Compiled all shaders")

add_custom_target(Assets COMMENT "Copied all assets")

add_custom_command(
  TARGET Assets
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_BINARY_DIR}/shaders"
          "${PROJECT_BINARY_DIR}/shaders"
  COMMENT "Copied shaders to output")

add_custom_command(
  TARGET Assets
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/models"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/res/models"
          "${PROJECT_BINARY_DIR}/models"
  COMMENT "Copied models to output")

add_custom_command(
  TARGET Assets
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/fonts"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/res/fonts"
          "${PROJECT_BINARY_DIR}/fonts"
  COMMENT "Copied fonts to output")

add_custom_command(
  TARGET Assets
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/textures"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${PROJECT_SOURCE_DIR}/res/textures" "${PROJECT_BINARY_DIR}/textures"
  COMMENT "Copied textures to output")

install(DIRECTORY "${PROJECT_BINARY_DIR}/shaders" TYPE DATA)
install(DIRECTORY "${PROJECT_BINARY_DIR}/textures" TYPE DATA)
install(DIRECTORY "${PROJECT_BINARY_DIR}/models" TYPE DATA)
install(DIRECTORY "${PROJECT_BINARY_DIR}/fonts" TYPE DATA)

add_subdirectory(engine)
add_subdirectory(games)
