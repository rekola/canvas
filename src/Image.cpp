#include <Image.h>

#include <ImageLoadingException.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cassert>

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

std::unique_ptr<ImageData>
Image::loadFromFile(const std::string & filename) {
  assert(!filename.empty());
  int w, h, channels;
  auto img_buffer = stbi_load(filename.c_str(), &w, &h, &channels, 0);
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
  if (size >= 4 && buffer[0] == 0xFF && buffer[1] == 0xD8 &&
      buffer[2] == 0xFF && buffer[3] == 0xDB) {
    return true;
  } else if (size >= 12 && buffer[0] == 0xFF && buffer[1] == 0xD8 &&
	     buffer[2] == 0xFF && buffer[3] == 0xE0 && buffer[4] == 0x00 &&
	     buffer[5] == 0x10 && buffer[6] == 0x4A && buffer[7] == 0x46 &&
	     buffer[8] == 0x49 && buffer[9] == 0x46 && buffer[10] == 0x00 &&
	     buffer[11] == 0x01) {
    return true;
  } else if (size >= 4 && buffer[0] == 0xFF && buffer[1] == 0xD8 &&
	     buffer[2] == 0xFF && buffer[3] == 0xEE) {
    return true;
  } else if (size >= 12 && buffer[0] == 0xFF && buffer[1] == 0xD8 &&
	     buffer[2] == 0xFF && buffer[3] == 0xE1 && buffer[6] == 0x45 &&
	     buffer[7] == 0x78 && buffer[8] == 0x69 && buffer[9] == 0x66 &&
	     buffer[10] == 0x00 && buffer[11] == 0x00) {
    return true;
  } else if (size >= 4 && buffer[0] == 0xFF && buffer[1] == 0xD8 &&
	     buffer[2] == 0xFF && buffer[3] == 0xE0) {
    return true;
  } else {
    return false;
  }      
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
