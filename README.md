canvas
======

[![Linux](https://github.com/rekola/canvas/actions/workflows/ci-linux.yml/badge.svg)](https://github.com/rekola/canvas/actions/workflows/ci-linux.yml) [![Linux ARM](https://github.com/rekola/canvas/actions/workflows/ci-linux-arm.yml/badge.svg)](https://github.com/rekola/canvas/actions/workflows/ci-linux-arm.yml) [![macOS](https://github.com/rekola/canvas/actions/workflows/ci-macos.yml/badge.svg)](https://github.com/rekola/canvas/actions/workflows/ci-macos.yml) [![Windows](https://github.com/rekola/canvas/actions/workflows/ci-windows.yml/badge.svg)](https://github.com/rekola/canvas/actions/workflows/ci-windows.yml)

A C++17 implementation of the HTML5 Canvas 2D API, drawing with each platform's own 2D library: Cairo on Linux, GDI+ on Windows, Quartz 2D on Apple platforms and Android Canvas on Android. It draws into memory and hands the pixels out as `ImageData`, so it can be used to render charts, meters and textures.

Version 0.2.0: breaking changes
===============================

0.2.0 does not keep source compatibility with 0.1.0. What to change:

* **Headers moved to `include/canvas/`.** Include `<canvas/Color.h>` instead of `<Color.h>`, `<canvas/ContextCairo.h>` instead of `<ContextCairo.h>`, and so on for every canvas header. The old names are not kept. Add `include/` (or the install prefix's `include/`) to the include path, which `canvas::canvas` does for you. This also stops canvas picking up a client's own `Color.h` or `Image.h`. Canvas's own headers include each other with quoted relative includes.
* **Gradients are values.** `auto & g = context->createLinearGradient(...)` no longer compiles; write `auto g = ...` and add the colour stops before assigning it to a style.
* **Images name their format.** `ImageData` says whether it is RGB or BGR and straight or premultiplied (see Pixel format), and `drawImage()` converts. Image names are no longer prefixed with `"assets/"`: give the factory a `FilenameConverter` to map names to paths.
* **The current path is in device coordinates.** Points are transformed when they are added (as in HTML5), `clip()` intersects with the existing clip and keeps the path, and `save()`/`restore()` leave the path alone.
* **Backends:** `Surface::renderPath()` takes a `LineStyle` in place of the dash vector, and `Surface::readPixels()` replaces reading the locked memory directly. Colour strings that are not understood give black instead of asserting.

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
`canvas::canvas`, installable and exported.

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

Float colour (HDR)
==================

A surface can hold 32-bit floats instead of bytes, so colours above 1 and in
the dark range survive:

    ContextCairo context(PixelSize{ 640, 360 }, 4, 1.0f, PixelType::FLOAT32);
    context.fillStyle(Color(4.0f, 0.001f, 0.25f, 1.0f)).fillRect(0, 0, 64, 64);
    auto pixels = context.getDefaultSurface().getImageData();   // PixelType::FLOAT32

`PixelType::UINT8` (the default) is the 8-bit API, unchanged. Ask
`Context::supportsPixelType()` or `ContextFactory::supportsPixelType()`
first; `createSurface(PixelSize, channels, PixelType)` and
`ContextFactory::createContext(PixelSize, channels, PixelType)` throw
`UnsupportedFormatException` when there is no such surface (float surfaces are
4 channel RGBA only).

**Canvas does not interpret the colour space.** `Color` is four floats, and
every operation is arithmetic on those numbers: blending (source over, copy),
anti-aliased edges, clipping, gradient interpolation (of the straight colour,
then premultiplied), image scaling. No backend applies a transfer function,
converts primaries or tone maps, and a float surface clamps nothing to [0, 1].
The caller supplies the values in the space it wants blended in: for physically
correct blending, linear light (cellui uses linear Rec.2020 with 1.0 as SDR
white, HDR above 1). Gamma-encoded values blend as gamma-encoded numbers.
Alpha is a coverage in [0, 1]. 8-bit surfaces clamp colours to [0, 1] as
before. Bytes drawn on a float surface (`drawImage()` of an 8-bit
`ImageData`) are `v / 255` as they are, so linearize them first if they are
encoded, and a float image drawn on a byte surface is clamped.

Read-back keeps the convention that the surface hands out its own layout,
tagged: `getImageData()` / `createImage()` give `ImageData` with
`PixelType::FLOAT32` (`getFloatData()`, 4 floats per pixel, rows without
padding), and on Cairo and Quartz the format is `RGB` order, **premultiplied**.
`convertTo(ChannelOrder::RGB, false)` makes it straight RGBA; nothing is
clamped on the way. The pixels are the numbers that were drawn (see the
precision notes below).

| Backend | Float surface | Notes |
| --- | --- | --- |
| Cairo | `CAIRO_FORMAT_RGBA128F` (1.17.2 or later) | See below: Cairo clamps its colour inputs, so canvas scales by a power of two ("headroom") |
| Quartz 2D | 32-bit float bitmap context, extended linear sRGB | macOS only; every colour, gradient and image is created in that one space, so nothing is converted. Compiled but not run locally; see CI |
| Direct2D | FP16 render target, scRGB-style linear values | Not in this repository; FP16 is widened to `FLOAT32` on read-back (`Half.h`) |
| GDI+ | none | `supportsPixelType()` is false; a float `ImageData` throws |
| Android | none | |

Cairo float surfaces
--------------------

Tested with Cairo 1.18.0 (pixman 0.42.2):

* Cairo **clamps every colour it is given to [0, 1]** (solid colours, gradient
  stops, and float image sources), although it stores and blends values above
  1 in an RGBA128F surface. A float `CairoSurface` therefore keeps its pixels
  divided by a "headroom": a power of two that grows to the largest straight
  colour component drawn so far (the stored pixels are rescaled when it does).
  Premultiplied source-over and copy are linear in the colour channels, and a
  power of two is exact in float, so the values are bit-identical. Read-back
  multiplies it back. `CairoSurface::getHeadroom()` tells it; code that reads
  `lockMemory()` or `getCairoSurface()` sees the stored values.
* Cairo's solid colours are 16-bit, so a float surface paints solids through a
  1x1 float image instead (exact). **Gradient stops are 16-bit** in pixman:
  stops and interpolation carry about `headroom / 65535` of absolute error,
  and a dark stop is quantized to that.
* **Anti-aliasing coverage is 8-bit** (Cairo's rasterizer): half coverage
  gives 128/255 of the value.
* Image scaling uses `CAIRO_FILTER_GOOD` on float surfaces. `BEST` is a
  fixed-point convolution that loses the dark range and darkens edges.
* Works and matches the 8-bit result within rounding: fills, strokes, dashes,
  anti-aliased and circular clips, text (grayscale and subpixel, glyph paths
  and `show_text`), linear and radial gradients, `COPY`, `SOURCE_OVER`,
  `paint_with_alpha`, groups and similar surfaces (they stay RGBA128F), and
  every Porter-Duff operator.
* Only the linear operators survive the headroom scaling. The PDF blend modes
  (multiply, screen, ...) are not scale invariant, and canvas does not use
  them.
* No filters (Cairo has none beyond the pattern filter above), no ICC.
* Cairo without float (before 1.17.2): `supportsPixelType(FLOAT32)` is false.

Image names
===========

`ContextFactory::loadImage(name)` loads `name` as given. Pass the factory a
`FilenameConverter` to map names to paths (an install directory, an app
bundle's resources); the Cairo, GDI+ and Quartz factories take one. Paths are
UTF-8 on every platform.

Palettes and dithering
======================

`quantize(image, palette, options)` (`Dither.h`) reduces an `ImageData` to
palette indices, one byte per pixel: what a 256-colour terminal or a Sixel
image takes. `Palette::xterm256()` is the xterm palette by colour number, or
give a `Palette` of up to 256 entries. It is always built, whatever
`CANVAS_TEXTURE_PACKING` says.

* `DitherMethod::ORDERED` (default) is a Bayer matrix (2, 4, 8 or 16 on a
  side). Each pixel is decided by its own colour and position, so changing
  one pixel changes only its output and an animated meter doesn't shimmer.
* `DitherMethod::FLOYD_STEINBERG` diffuses the error: smoother, but a change
  carries to everything after it.
* `DitherMethod::NONE` takes the nearest entry.

The image is read by its format; translucent pixels are composited over
`options.background*` first (black by default). `options.firstIndex = 16`
leaves out the xterm system colours, which terminals let the user change.
One channel is taken as a grey level here, not as an alpha mask.

Geometry
========

As in HTML5 canvas, integer coordinates are pixel edges and pixel centres are
at .5. A 1 px line on an integer coordinate is antialiased over two rows; put
it on a .5 coordinate to get one crisp row. Fills with integer edges, and
images drawn at integer positions and native size, are exact.

Display scale
=============

A context's size and coordinates are logical units; the display scale
(`ContextCairo(width, height, channels, scale)`, or the factory's) is how
many pixels a unit covers. Backends multiply coordinates, line widths, font
sizes, gradients, dashes and shadows by it. Given a logical size, the surface
gets `logical * scale` pixels, rounded down.

To get an exact pixel size, such as an image covering N terminal cells at a
fractional scale, give a `PixelSize` instead: the logical size is then
`pixels / scale` and may be fractional.

    ContextCairo context(PixelSize{ 101, 37 }, 4, 1.25f);  // 101 x 37 pixels
    context.getWidth();         // 80.8 logical units
    context.getActualWidth();   // 101
    context.resize(PixelSize{ 120, 40 });
    auto layer = context.createSurface(PixelSize{ 20, 20 }, 4);

`getWidth()`/`getHeight()` and `Surface::getLogicalWidth()`/`getLogicalHeight()`
are `float`s for this reason; they are whole numbers for a context made from a
logical size. Factories have `createContext(PixelSize)` and
`createSurface(PixelSize)`, and `Image::scale()` takes a fractional logical size. A backend
implements these overloads, and its `Surface` subclass takes `float` logical
sizes in its constructor and `resize()`.

Hairlines
=========

`lineWidth = 0` is a hairline: one device pixel wide whatever the display
scale and transform, for grid lines, ticks and borders that must not thicken
on a high-DPI display. This is how PostScript, Quartz and GDI+ treat a zero
width; HTML5 ignores 0 instead. It works for `stroke()`, `strokeRect()` and
`strokeText()`, and the other line settings (caps, joins, dashes) still apply.
Dash lengths stay in user units.

To keep a hairline crisp, put it on a device pixel centre: logical
`(n + 0.5) / scale` without a transform.

    context.lineWidth(0.0f).beginPath().moveTo(0, 5.5 / scale).lineTo(width, 5.5 / scale).stroke();

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
