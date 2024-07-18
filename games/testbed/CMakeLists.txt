file(GLOB_RECURSE sources ./*.c)

add_library(testbed MODULE ${sources})
target_link_libraries(testbed PRIVATE engine_lib)

add_custom_command(
  TARGET testbed
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:testbed> ${PROJECT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E rename ${PROJECT_BINARY_DIR}/libtestbed.so
          ${PROJECT_BINARY_DIR}/libgame.so
  COMMENT "Copying testbed")

install(
  TARGETS testbed
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
