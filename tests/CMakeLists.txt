add_executable(imagedata_tests imagedata_tests.cpp)
target_link_libraries(imagedata_tests PRIVATE canvas)
add_test(NAME canvas_imagedata COMMAND imagedata_tests)

add_executable(dither_tests dither_tests.cpp)
target_link_libraries(dither_tests PRIVATE canvas)
add_test(NAME canvas_dither COMMAND dither_tests)

add_executable(path_tests path_tests.cpp)
target_link_libraries(path_tests PRIVATE canvas)
add_test(NAME canvas_path COMMAND path_tests)

if(CANVAS_BACKEND_CAIRO)
  add_executable(guard_tests guard_tests.cpp)
  target_link_libraries(guard_tests PRIVATE canvas)
  add_test(NAME canvas_guards COMMAND guard_tests)

endif()

if(CANVAS_BACKEND_CAIRO OR CANVAS_BACKEND_GDIPLUS OR CANVAS_BACKEND_QUARTZ)
  add_executable(canvas_tests pixel_tests.cpp)
  target_link_libraries(canvas_tests PRIVATE canvas)
  add_test(NAME canvas_pixels COMMAND canvas_tests)
endif()
