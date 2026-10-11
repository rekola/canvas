add_executable(imagedata_tests imagedata_tests.cpp)
target_link_libraries(imagedata_tests PRIVATE canvas)
add_test(NAME canvas_imagedata COMMAND imagedata_tests)

if(CANVAS_BACKEND_CAIRO)
  add_executable(canvas_tests pixel_tests.cpp)
  target_link_libraries(canvas_tests PRIVATE canvas)
  add_test(NAME canvas_pixels COMMAND canvas_tests)
endif()
