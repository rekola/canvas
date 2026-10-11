#ifndef _CANVAS_INTERNALFORMAT_H_
#define _CANVAS_INTERNALFORMAT_H_

namespace canvas {  
  enum class InternalFormat {
    NO_FORMAT = 0,
    R8,
    RG8,
    RGB565,
    RGBA4,
    RGBA8,
    RGB8,
    RED_RGTC1,
    RG_RGTC2,
    RGB_DXT1,
    RGBA_DXT5,
    RGB_ETC1,
    LUMINANCE_ALPHA,
    LA44, // not a real OpenGL format
    R32F,
    RGB555,
    RGBA5551
  };

  static inline bool has_alpha(InternalFormat internal_format) {
    switch (internal_format) {
    case InternalFormat::R8:
    case InternalFormat::RGB_ETC1:
    case InternalFormat::RGB_DXT1:
    case InternalFormat::RGB565:
      return false;
      
    case InternalFormat::RG8:
    case InternalFormat::RGBA4:
    case InternalFormat::RGBA8:
    case InternalFormat::LUMINANCE_ALPHA:
    default:
      return true;
    }
  }
};

#endif
