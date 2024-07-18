find_package(glfw3 3.3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(cglm REQUIRED)

file(GLOB_RECURSE engine_sources core/*.c)
file(GLOB_RECURSE runtime_sources runtime/*.c)

add_library(engine_lib ${engine_sources})
target_link_libraries(engine_lib glfw ${Vulkan_LIBRARY} cglm m)
target_include_directories(engine_lib PRIVATE ${Vulkan_INCLUDE_DIR}
                                              ${STB_INCLUDE_PATH})
target_include_directories(engine_lib PUBLIC core)

add_dependencies(engine_lib Shaders Assets)

add_executable(runtime ${runtime_sources})
target_link_libraries(runtime engine_lib)

add_custom_command(
  TARGET runtime
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:runtime> ${PROJECT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E rename ${PROJECT_BINARY_DIR}/runtime
          ${PROJECT_BINARY_DIR}/run
  COMMENT "Copying runtime")

install(TARGETS engine_lib)
install(TARGETS runtime)
