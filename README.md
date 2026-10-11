canvas
======

A C++ canvas implementation.

Currently this library is used to create OpenGL textures with an interface which is based on HTML5 Canvas.

On Windows, rendering is done using GDI+, Cairo is used on Linux, Quartz2D is used on iOS and Android Canvas is used on Android.

Fluent Style
============

Fluent programming style is supported in this implementation, even though it's not available in the original HTML5 Canvas:

    context->strokeStyle("#ff0000").moveTo(10, 10).lineTo(20, 20).stroke();

Example
=======

    auto context = new ContextCairo(width, height);
    context->moveTo(10, 10);
    context->lineTo(20, 20);
    context->strokeStyle = "#ff0000";
    context->stroke();

Build
=====

    cmake -B build && cmake --build build && ctest --test-dir build

One option per backend: `CANVAS_BACKEND_CAIRO` (default on), `CANVAS_BACKEND_GDIPLUS`,
`CANVAS_BACKEND_QUARTZ`, `CANVAS_BACKEND_ANDROID` (default off). The target is
`canvas::canvas`, installable and exported. `PerlinSurface` no longer compiles
and is not part of the build.

Pixel format
============

`ImageData` is a plain byte array, `width * height * channels`, rows top to
bottom with no padding. What the bytes mean depends on where it came from:

* Read back from a Cairo surface (`Surface::createImage()`): the surface's
  own memory, 4 channels, **premultiplied** alpha, one 32-bit native-endian
  word per pixel as `0xAARRGGBB`. On little-endian machines the bytes in
  memory are therefore **B, G, R, A**.
* Loaded from a file (`Image::loadFromFile()`): the file's channels in
  order (R, G, B[, A]) with **straight** alpha. `ImageData::multiplyAlpha()`
  converts straight RGBA to premultiplied.

Cairo geometry
==============

Path coordinates and drawn images are offset by half a pixel, so a 1 px line
at an integer coordinate is crisp. A fill with integer edges is therefore
antialiased on its edges, and an image sits half a pixel off.

Linux
=====

For input images, only PNG-files are supported.

Windows
=======

Quality of clipping is very bad since it's done per pixel with no anti-aliasing.

Android
=======

On Android the canvas is implemented using JNI, which is used to call Java methods. Due to this, performance might not be optimal.

Credits
=======

* Mikael Rekola
* Mikko Suni (for Android support)
