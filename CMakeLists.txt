cmake_minimum_required(VERSION 3.16)
project(canvas VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CANVAS_BACKEND_CAIRO "Cairo backend (Linux)" ON)
option(CANVAS_BACKEND_GDIPLUS "GDI+ backend (Windows)" OFF)
option(CANVAS_BACKEND_QUARTZ "Quartz 2D backend (Apple)" OFF)
option(CANVAS_BACKEND_ANDROID "Android Canvas backend (JNI)" OFF)
option(CANVAS_TEXTURE_PACKING "PackedImageData: GL texture formats, DXT and ETC1 compression" OFF)
option(CANVAS_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})

# The core. Sources include their headers as <Name.h>, so the include
# directory is public.
add_library(canvas
  src/Color.cpp
  src/ColorProvider.cpp
  src/Image.cpp
  src/ImageData.cpp
  src/Path2D.cpp
  )
add_library(canvas::canvas ALIAS canvas)

if(CANVAS_TEXTURE_PACKING)
  target_sources(canvas PRIVATE src/FloydSteinberg.cpp src/PackedImageData.cpp src/dxt.cpp src/rg_etc1.cpp)
  target_compile_definitions(canvas PUBLIC CANVAS_HAS_TEXTURE_PACKING)
endif()
target_include_directories(canvas PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/canvas>)

if(CANVAS_BACKEND_CAIRO)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
  target_sources(canvas PRIVATE src/ContextCairo.cpp)
  target_link_libraries(canvas PUBLIC PkgConfig::CAIRO)
  target_compile_definitions(canvas PUBLIC CANVAS_HAS_CAIRO)
endif()

if(CANVAS_BACKEND_GDIPLUS)
  target_sources(canvas PRIVATE src/ContextGDIPlus.cpp)
  target_link_libraries(canvas PUBLIC gdiplus)
  target_compile_definitions(canvas PUBLIC CANVAS_HAS_GDIPLUS)
endif()

if(CANVAS_BACKEND_QUARTZ)
  target_sources(canvas PRIVATE src/ContextQuartz2D.cpp)
  target_compile_definitions(canvas PUBLIC CANVAS_HAS_QUARTZ)
endif()

if(CANVAS_BACKEND_ANDROID)
  target_sources(canvas PRIVATE src/ContextAndroid.cpp)
  target_compile_definitions(canvas PUBLIC CANVAS_HAS_ANDROID)
endif()

install(TARGETS canvas EXPORT canvasTargets ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/canvas)
install(EXPORT canvasTargets NAMESPACE canvas:: DESTINATION lib/cmake/canvas)

if(CANVAS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
