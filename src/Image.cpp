#include <Image.h>

#include <ImageLoadingException.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cassert>
#include <cstdio>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace std;
using namespace canvas;

void
Image::decode(const void * buffer, size_t size) {
  data = loadFromMemory(buffer, size);
}

std::unique_ptr<ImageData>
Image::loadFromMemory(const void * buffer, size_t size) {
  int w, h, channels;
  auto img_buffer = stbi_load_from_memory(reinterpret_cast<const unsigned char *>(buffer), size, &w, &h, &channels, 0);
  if (!img_buffer) {
    throw ImageLoadingException(stbi_failure_reason());
  }
  // cerr << "Image.cpp: loaded image, size = " << size << ", b = " << (void*)img_buffer << ", w = " << w << ", h = " << h << ", ch = " << channels << endl;
  assert(w && h && channels);    

  auto data = std::make_unique<ImageData>((unsigned char *)img_buffer, w, h, channels);
  
  stbi_image_free(img_buffer);
  
  return data;
}

static FILE * openUtf8(const std::string & filename) {
#ifdef _WIN32
  int n = MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), -1, nullptr, 0);
  if (n <= 0) return nullptr;
  std::vector<wchar_t> wide(n);
  MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), -1, wide.data(), n);
  return _wfopen(wide.data(), L"rb");
#else
  return fopen(filename.c_str(), "rb");
#endif
}

std::unique_ptr<ImageData>
Image::loadFromFile(const std::string & filename) {
  assert(!filename.empty());
  int w, h, channels;
  // The name is UTF-8 on every platform; the Windows C runtime's fopen()
  // would read it in the ANSI code page.
  FILE * f = openUtf8(filename);
  if (!f) {
    throw ImageLoadingException(("unable to open " + filename).c_str());
  }
  auto img_buffer = stbi_load_from_file(f, &w, &h, &channels, 0);
  fclose(f);
  if (!img_buffer) {
    throw ImageLoadingException(stbi_failure_reason());
  }
  assert(w && h && channels);    

  auto data = std::make_unique<ImageData>((unsigned char *)img_buffer, w, h, channels);
  
  stbi_image_free(img_buffer);

  return data;
}

bool
Image::isPNG(const unsigned char * buffer, size_t size) {
  return size >= 4 && buffer[0] == 0x89 && buffer[1] == 0x50 && buffer[2] == 0x4e && buffer[3] == 0x47;
}

bool
Image::isJPEG(const unsigned char * buffer, size_t size) {
  // SOI followed by any marker: JFIF (E0), Exif (E1), Adobe (EE), plain (DB).
  return size >= 3 && buffer[0] == 0xFF && buffer[1] == 0xD8 && buffer[2] == 0xFF;
}

bool
Image::isGIF(const unsigned char * buffer, size_t size) {
  return size >= 6 && buffer[0] == 'G' && buffer[1] == 'I' && buffer[2] == 'F' && buffer[3] == '8' && (buffer[4] == '7' || buffer[4] == '9') && buffer[5] == 'a';
}

bool
Image::isBMP(const unsigned char * buffer, size_t size) {
  return size > 2 && buffer[0] == 0x42 && buffer[1] == 0x4d;
}

bool
Image::isXML(const unsigned char * buffer, size_t size) {
  return size >= 6 && buffer[0] == '<' && buffer[1] == '!' && buffer[2] == 'D' && buffer[3] == 'O' && buffer[4] == 'C' && buffer[5] == 'T';
}
