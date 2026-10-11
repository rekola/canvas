add_executable(canvas_tests pixel_tests.cpp)
target_link_libraries(canvas_tests PRIVATE canvas)
add_test(NAME canvas_pixels COMMAND canvas_tests)
