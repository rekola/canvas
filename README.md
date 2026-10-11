canvas
======

A C++ canvas implementation.

Currently this library is used to create OpenGL textures with an interface which is based on HTML5 Canvas.

On Windows, rendering is done using GDI+, Cairo is used on Linux, Quartz2D is used on iOS and Android Canvas is used on Android.

Fluent Style
============

Fluent programming style is supported in this implementation, even though it's not available in the original HTML5 Canvas: setters, transforms and path calls all return the context.

    context->strokeStyle("#ff0000").lineWidth(2.0f).translate(5, 5).beginPath().moveTo(10, 10).lineTo(20, 20).stroke();

Gradients
=========

`createLinearGradient()` returns a new gradient each time. Unlike HTML5's `CanvasGradient` it is a value, so add the colour stops before assigning it:

    auto gradient = context->createLinearGradient(0, 0, 100, 0);
    gradient.addColorStop(0.0f, "#000000");
    gradient.addColorStop(1.0f, "#ffffff");
    context->fillStyle = gradient;

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
`CANVAS_BACKEND_QUARTZ`, `CANVAS_BACKEND_ANDROID` (default off). `CANVAS_TEXTURE_PACKING` (default off)
adds `PackedImageData` (OpenGL texture formats, DXT and ETC1 compression). The target is
`canvas::canvas`, installable and exported. `PerlinSurface` no longer compiles
and is not part of the build.

Pixel format
============

`ImageData` is a plain byte array, `width * height * channels`, rows top to
bottom with no padding, and says what the bytes are:
`getChannelOrder()` (`RGB` or `BGR`; alpha is last) and `isPremultiplied()`.
`convertTo()` changes the bytes to another format, `setFormat()` only relabels
them. An `ImageData` is straight RGBA unless set otherwise:

* Loaded from a file (`Image::loadFromFile()`): the file's channels in order
  (R, G, B[, A]) with **straight** alpha. `ImageData::multiplyAlpha()`
  converts straight RGBA to premultiplied.
* Read back from a Cairo surface (`Surface::createImage()`): the surface's
  own memory, 4 channels, **premultiplied** alpha, one 32-bit native-endian
  word per pixel as `0xAARRGGBB`. On little-endian machines the bytes in
  memory are therefore **B, G, R, A**, and the image is marked `BGR`,
  premultiplied. Rows are copied without Cairo's padding.
* One channel is an alpha mask (what shadows use), whatever its source.

`drawImage()` converts from the image's format, so a loaded PNG with alpha
draws correctly.

Image names
===========

`ContextFactory::loadImage(name)` loads `name` as given. Pass the factory a
`FilenameConverter` to map names to paths (an install directory, an app
bundle's resources); the Cairo, GDI+ and Quartz factories take one. Paths are
UTF-8 on every platform.

Geometry
========

As in HTML5 canvas, integer coordinates are pixel edges and pixel centres are
at .5. A 1 px line on an integer coordinate is antialiased over two rows; put
it on a .5 coordinate to get one crisp row. Fills with integer edges, and
images drawn at integer positions and native size, are exact.

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
