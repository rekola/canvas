#include "OpenGLTexture.h"

#include "TextureRef.h"

#ifdef WIN32
#include <GL/glew.h>
#endif

#define GL_GLEXT_PROTOTYPES

#ifdef __APPLE__
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>
#else
// #include <GLES3/gl3.h>
// #include <EGL/egl.h>
// #include <EGL/eglext.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __WXMAC__
#include "OpenGL/gl.h"
#else
#include <GL/gl.h>
#endif

#ifdef _WIN32
#include "glext.h"
#else
#include <GL/glext.h>
#endif

#endif

#include <cassert>

using namespace std;
using namespace canvas;

size_t OpenGLTexture::total_textures = 0;
vector<unsigned int> OpenGLTexture::freed_textures;

static GLenum getOpenGLInternalFormat(InternalFormat internal_format) {
  switch (internal_format) {
  case RG8: return GL_RG8;
  case RGB5: return GL_RGB565;
  case RGBA4: return GL_RGBA4;
  case RGBA8: return GL_RGBA8;
#ifdef __linux__
  case COMPRESSED_RG: return GL_RG8;
  case COMPRESSED_RGB: return GL_RGB5;
  case COMPRESSED_RGBA: return GL_RGBA8;
#else
  case COMPRESSED_RG: return GL_COMPRESSED_RG11_EAC;
  case COMPRESSED_RGB: return GL_COMPRESSED_RGB8_ETC2;
  case COMPRESSED_RGBA: return GL_COMPRESSED_RGBA8_ETC2_EAC;
#endif
  case LUMINANCE_ALPHA: return GL_LUMINANCE_ALPHA;
  }
  return 0;
}

static GLenum getOpenGLFilterType(FilterMode mode) {
  switch (mode) {
  case NEAREST: return GL_NEAREST;
  case LINEAR: return GL_LINEAR;
  case LINEAR_MIPMAP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
  }
  return 0;
}

void
OpenGLTexture::updateData(const void * buffer) {
  updateData(buffer, 0, 0, getActualWidth(), getActualHeight());
}

void
OpenGLTexture::updateData(const void * buffer, unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
  assert(buffer);

  bool initialize = false;
  if (!texture_id) {
    initialize = true;
    glGenTextures(1, &texture_id);
    if (texture_id >= 1) total_textures++;    
  }
  assert(texture_id >= 1);
  
  glBindTexture(GL_TEXTURE_2D, texture_id);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  bool has_mipmaps = getMinFilter() == LINEAR_MIPMAP_LINEAR;
  if (initialize) {
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWidth(), getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);    
    glTexStorage2D(GL_TEXTURE_2D, has_mipmaps ? getMipmapLevels() : 1, getOpenGLInternalFormat(getInternalFormat()), getActualWidth(), getActualHeight());

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, getOpenGLFilterType(getMinFilter()));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, getOpenGLFilterType(getMagFilter()));
  }

#ifdef __APPLE__
  glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
#else
  glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, buffer);
#endif
  if (has_mipmaps) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  glBindTexture(GL_TEXTURE_2D, 0);
}

void
OpenGLTexture::releaseTextures() {
  // cerr << "DELETING TEXTURES: " << OpenGLTexture::getFreedTextures().size() << "/" << OpenGLTexture::getNumTextures() << endl;
  
  for (vector<unsigned int>::const_iterator it = freed_textures.begin(); it != freed_textures.end(); it++) {
    GLuint texid = *it;
    glDeleteTextures(1, &texid);
  }
  freed_textures.clear();
}

TextureRef
OpenGLTexture::createTexture(unsigned int _logical_width, unsigned int _logical_height, unsigned int _actual_width, unsigned int _actual_height, FilterMode min_filter, FilterMode mag_filter, InternalFormat _internal_format, unsigned int mipmap_levels) {
  return TextureRef(_logical_width, _logical_height, _actual_width, _actual_height, new OpenGLTexture(_logical_width, _logical_height, _actual_width, _actual_height, min_filter, mag_filter, _internal_format, mipmap_levels));
}
