#include <canvas/PackedImageData.h>

#include <canvas/FloydSteinberg.h>
#include <canvas/ImageData.h>

#include "rg_etc1.h"
#include "dxt.h"

#include <cassert>

using namespace std;
using namespace canvas;

bool PackedImageData::etc1_initialized = false;

PackedImageData::PackedImageData(InternalFormat format, unsigned short levels, const ImageData & input)
  : format_(format), width_(input.getWidth()), height_(input.getHeight()), levels_(levels)
{
  if (format_ == InternalFormat::NO_FORMAT) {
    if (input.getNumChannels() == 4) format_ = InternalFormat::RGBA8;
    else if (input.getNumChannels() == 3) format_ = InternalFormat::RGB8;
    else if (input.getNumChannels() == 2) format_ = InternalFormat::RG8;
    else if (input.getNumChannels() == 1) format_ = InternalFormat::R8;
    else {
      assert(0);
    }
  }

  unsigned short num_channels = input.getNumChannels();
  unsigned int bytesPerRow = getBytesPerRow(), bytesPerPixel = getBytesPerPixel();
    
  auto s = calculateSize();
  data_ = std::make_unique<unsigned char[]>(s);
  memset(data_.get(), 0, s);
  
  if ((num_channels == 4 && (format == InternalFormat::RGB8 || format == InternalFormat::RGBA8)) ||
      (num_channels == 1 && format == InternalFormat::R8) ||
      (num_channels == 2 && format == InternalFormat::RG8)) {
    assert(levels == 1);
    for (unsigned int row = 0; row < height_; row++) {
      memcpy(data_.get() + row * bytesPerRow, input.getData() + row * width_ * bytesPerPixel, bytesPerRow);
    }
  } else if (format == InternalFormat::RGBA4 || format == InternalFormat::RGB565 || format == InternalFormat::RGB555 || format == InternalFormat::RGBA5551) {
    FloydSteinberg fs(format);
    auto offset = fs.apply(input, data_.get(), getBytesPerRow(input.getWidth(), format));
    if (levels >= 2) {
      auto img = input.scale((input.getWidth() + 1) / 2, (input.getHeight() + 1) / 2);
      for (unsigned int l = 1; l < levels; l++) {
	offset += fs.apply(*img, data_.get() + offset, getBytesPerRow(img->getWidth(), format));
	if (l + 1 < levels) {
	  img = img->scale((img->getWidth() + 1) / 2, (img->getHeight() + 1) / 2);
	}
      }
    }
  } else {
    assert(levels == 1);
    
    auto input_data = input.getData();

    if (format == InternalFormat::RGB8 || format == InternalFormat::RGBA8) {
      if (num_channels == 3) {
	for (unsigned int row = 0, i = 0; row < height_; row++) {
	  auto ptr = (unsigned int *)(data_.get() + row * bytesPerRow);
	  for (unsigned int col = 0; col < width_; col++, i += 3) {
	    *ptr++ = (0xff << 24) | (input_data[i] << 16) | (input_data[i + 1] << 8) | (input_data[i + 2]);
	  }
	}
      } else if (num_channels == 1) {
	for (unsigned int row = 0, i = 0; row < height_; row++) {
	  auto ptr = (unsigned int *)(data_.get() + row * bytesPerRow);
	  for (unsigned int col = 0; col < width_; col++, i++) {
	    unsigned char v = input_data[i];
	    *ptr++ = (0xff << 24) | (v << 16) | (v << 8) | (v);
	  }
	}
      } else {
	assert(0);
      }
    } else if (format == InternalFormat::LA44) {
      for (unsigned int row = 0, i = 0; row < height_; row++) {
	auto ptr = (unsigned char *)(data_.get() + row * bytesPerRow);
	for (unsigned int col = 0; col < width_; col++, i += num_channels) {
	  auto r = input_data[i];
	  auto g = num_channels >= 2 ? input_data[i + 1] : r;
	  auto b = num_channels >= 3 ? input_data[i + 2] : g;
	  auto a = (num_channels >= 4 ? input_data[i + 3] : 0xff) >> 4;
	  auto lum = ((r + g + b) / 3) >> 4;
	  if (lum >= 16) lum = 15;
	  *ptr++ = (a << 4) | lum;
	}
      }
    } else if (format == InternalFormat::RG8) {
      for (unsigned int row = 0, i = 0; row < height_; row++) {
	auto ptr = (unsigned char *)(data_.get() + row * bytesPerRow);
	for (unsigned int col = 0; col < width_; col++, i += num_channels) {
	  auto r = input_data[i];
	  auto g = num_channels >= 2 ? input_data[i + 1] : r;
	  auto b = num_channels >= 3 ? input_data[i + 2] : g;
	  auto a = num_channels >= 4 ? input_data[i + 3] : 0xff;
	  auto lum = (r + g + b) / 3;
	  if (lum > 255) lum = 255;
	  *ptr++ = lum;
	  *ptr++ = a;
	}
      }      
    } else {
      // cerr << "unable to pack input data (channels = " << input.getNumChannels() << ", f = " << int(format) << ")\n";
      assert(0);
    }
  }
}

PackedImageData::PackedImageData(InternalFormat format, unsigned short width, unsigned short height, unsigned short levels, const unsigned char * input)
  : width_(width), height_(height), levels_(levels), format_(format) {
  size_t s = calculateSize();
  data_ = std::make_unique<unsigned char[]>(s);
  if (input) {
    memcpy(data_.get(), input, s);
  } else {
    if (format == InternalFormat::RGB_ETC1) {
      for (unsigned int i = 0; i < s; i += 8) {
	*(unsigned int *)(data_.get() + i + 0) = 0x00000000;
	*(unsigned int *)(data_.get() + i + 4) = 0xffffffff;
      }
    } else if (format == InternalFormat::RGB_DXT1) {
      for (unsigned int i = 0; i < s; i += 8) {
	*(unsigned int *)(data_.get() + i + 0) = 0x00000000;
	*(unsigned int *)(data_.get() + i + 4) = 0xaaaaaaaa;
      }
    } else if (format == InternalFormat::RED_RGTC1) {
      for (unsigned int i = 0; i < s; i += 8) {
	*(unsigned int *)(data_.get() + i + 0) = 0x00000003; // doesn't work on big endian
	*(unsigned int *)(data_.get() + i + 4) = 0x00000000;
      }
    } else if (format == InternalFormat::RG_RGTC2) {
      for (unsigned int i = 0; i < s; i += 16) {
	*(unsigned int *)(data_.get() + i + 0) = 0x00000003;
	*(unsigned int *)(data_.get() + i + 4) = 0x00000000;
	*(unsigned int *)(data_.get() + i + 8) = 0x00000003;
	*(unsigned int *)(data_.get() + i + 12) = 0x00000000;
      }
    } else {
      memset(data_.get(), 0, s);
    }
  }
}

#if 0
void
PackedImageData::createMipmaps(const ImageData & input_data, unsigned short target_levels) const {
  auto & fd = getImageFormat(format);
  assert(fd.getBytesPerPixel() == 4);
  assert(!fd.getCompression());
  assert(levels == 1);
  size_t target_size = calculateOffset(target_levels);
  auto output_data = std::make_unique<unsigned char[]>(target_size);
  memcpy(output_data_.get(), data_.get(), calculateOffset(1));
  unsigned short source_width = width, source_height = height;
  unsigned short target_width = width / 2, target_height = height / 2;
  for (unsigned int level = 1; level < target_levels; level++) {
    unsigned char * source_data = output_data_.get() + calculateOffset(level - 1);
    unsigned char * target_data = output_data_.get() + calculateOffset(level);
    for (unsigned int y = 0; y < target_height; y++) {               
      for (unsigned int x = 0; x < target_width; x++) {
	unsigned int source_offset = (2 * y * source_width + 2 * x) * 4;
	unsigned int target_offset = (y * target_width + x) * 4;
	for (unsigned int c = 0; c < 4; c++) {
	  target_data[target_offset] = ((unsigned int)source_data[source_offset]  + 
					(unsigned int)source_data[source_offset + 4]  + 
					(unsigned int)source_data[source_offset + 4 + 4 * source_width] +
					(unsigned int)source_data[source_offset + 4 * source_width]) / 4; 
	  source_offset++;
	  target_offset++;
	}
      }
    }
    source_width = target_width;
    source_height = target_height;
    target_width /= 2;
    target_height /= 2;
  }
  return make_unique<ImageData>(output_data_.get(), getInternalFormat(), width, height, target_levels);
}

std::unique_ptr<ImageData>
ImageData::convert(InternalFormat target_format) const {
  auto & fd = getImageFormat(format);
  
  assert(!fd.getCompression());

  if (target_fd.getCompression() == ImageFormat::DXT1 || target_fd.getCompression() == ImageFormat::ETC1 || target_fd.getCompression() == ImageFormat::RGTC1 || target_fd.getCompression() == ImageFormat::RGTC2) {
    rg_etc1::etc1_pack_params params;
    params.m_quality = rg_etc1::cLowQuality;
    if (target_fd.getCompression() == ImageFormat::ETC1 && !etc1_initialized) {
      // cerr << "initializing etc1" << endl;
      etc1_initialized = true;
      rg_etc1::pack_etc1_block_init();
    }
    assert((width & 3) == 0);
    assert((height & 3) == 0);
    unsigned short target_width = width, target_height = height;
    unsigned int target_size = calculateSize(target_width, target_height, levels, target_format);
    auto output_data = std::make_unique<unsigned char[]>(target_size);
    unsigned char input_block[4*4*8];
    unsigned int target_offset = 0;
    for (unsigned int level = 0; level < levels; level++) {
      unsigned int rows = (target_height + 3) / 4, cols = (target_width + 3) / 4;
      unsigned int base_source_offset = calculateOffset(level);
      // cerr << "compressing texture, level " << level << ", rows = " << rows << ", cols = " << cols << "\n";
      for (unsigned int row = 0; row < rows; row++) {
	for (unsigned int col = 0; col < cols; col++) {
	  for (unsigned int y = 0; y < 4; y++) {
	    for (unsigned int x = 0; x < 4; x++) {
	      unsigned int source_offset = base_source_offset + ((row * 4 + y) * target_width + col * 4 + x) * fd.getBytesPerPixel();
	      unsigned char r = data[source_offset++];
	      unsigned char g = fd.getBytesPerPixel() >= 2 ? data[source_offset++] : r;
	      unsigned char b = fd.getBytesPerPixel() >= 3 ? data[source_offset++] : g;
	      unsigned char a = fd.getBytesPerPixel() >= 4 ? data[source_offset++] : 0xff;
	      if (target_fd.getCompression() == ImageFormat::ETC1) {
		unsigned int offset = (y * 4 + x) * 4;
		input_block[offset++] = r;
		input_block[offset++] = g;
		input_block[offset++] = b;
		input_block[offset++] = 255; // data[source_offset++];
	      } else if (target_fd.getCompression() == ImageFormat::DXT1) {
		unsigned int offset = (y * 4 + x) * 4;
		input_block[offset++] = b;
		input_block[offset++] = g;
		input_block[offset++] = r;
		input_block[offset++] = 255; // data[source_offset++];
	      } else if (target_fd.getCompression() == ImageFormat::RGTC1) {
		unsigned int offset = y * 4 + x;
		input_block[offset] = r;
	      } else {
		unsigned int offset = y * 4 + x;
		input_block[offset] = r;
		input_block[offset + 16] = a;
	      }
	    }
	  }
	  if (target_fd.getCompression() == ImageFormat::ETC1) {
	    rg_etc1::pack_etc1_block(output_data_.get() + target_offset, (const unsigned int *)&(input_block[0]), params);	  
	    target_offset += 8;
	  } else if (target_fd.getCompression() == ImageFormat::DXT1) {
	    stb_compress_dxt1_block(output_data_.get() + target_offset, &(input_block[0]), false, 2);
	    target_offset += 8;
	  } else if (target_fd.getCompression() == ImageFormat::RGTC1) {
	    stb_compress_rgtc1_block(output_data_.get() + target_offset, &(input_block[0]));
	    target_offset += 8;
	  } else {
	    stb_compress_rgtc2_block(output_data_.get() + target_offset, &(input_block[0]));
	    target_offset += 16;
	  }
	}
      }
      target_width = (target_width + 1) / 2;
      target_height = (target_height + 1) / 2;
    }
    return make_unique<ImageData>(output_data_.get(), target_format, width, height, levels);
  } else if (target_fd.getNumChannels() == 1) {
    assert(levels == 1);
    
    unsigned int n = width * height;
    auto tmp = std::make_unique<unsigned char[]>(target_fd.getBytesPerPixel() * n);
    unsigned char * output_data = (unsigned char *)tmp.get();
    
    for (unsigned int i = 0; i < n; i++) {
      unsigned int input_offset = i * fd.getBytesPerPixel();
      unsigned char r = data[input_offset++];
      unsigned char g = fd.getBytesPerPixel() >= 1 ? data[input_offset++] : r;
      unsigned char b = fd.getBytesPerPixel() >= 2 ? data[input_offset++] : g;
      *output_data++ = (r + g + b) / 3;
    }

    return make_unique<ImageData>(tmp.get(), target_format, getWidth(), getHeight());
  } else {
    assert(target_fd.getBytesPerPixel() == 2);
    auto target_size = calculateSize(getWidth(), getHeight(), getLevels(), target_format);
    auto tmp = std::make_unique<unsigned char[]>(target_size);
    auto output_data = reinterpret_cast<unsigned short *>(tmp.get());
    auto n = calculateSize() / fd.getBytesPerPixel();
    if (target_fd.getNumChannels() == 2) {
      for (auto i = 0; i < n; i++) {
	unsigned int input_offset = i * fd.getBytesPerPixel();
	unsigned char r = data[input_offset++];
	unsigned char g = fd.getBytesPerPixel() >= 1 ? data[input_offset++] : r;
	unsigned char b = fd.getBytesPerPixel() >= 2 ? data[input_offset++] : g;
	unsigned char a = fd.getBytesPerPixel() >= 3 ? data[input_offset++] : 0xff;
	unsigned int lum = (r + g + b) / 3;
	if (lum >= 255) lum = 255;
	*output_data++ = (a << 8) | lum;
      }
    } else {
      for (auto i = 0; i < n; i++) {
	unsigned int input_offset = i * fd.getBytesPerPixel();
	unsigned char r = data[input_offset++] >> 4;
	unsigned char g = (fd.getBytesPerPixel() >= 1 ? data[input_offset++] : r) >> 4;
	unsigned char b = (fd.getBytesPerPixel() >= 2 ? data[input_offset++] : g) >> 4;
	unsigned char a = (fd.getBytesPerPixel() >= 3 ? data[input_offset++] : 0xff) >> 4;
#if defined __APPLE__ || defined __ANDROID__
	*output_data++ = (r << 12) | (g << 8) | (b << 4) | a;
#else
	*output_data++ = (b << 12) | (g << 8) | (r << 4) | a;
#endif
      }
    }
    
    return make_unique<ImageData>(tmp.get(), target_format, getWidth(), getHeight(), getLevels());
  }
}

std::unique_ptr<ImageData>
ImageData::scale(unsigned short target_base_width, unsigned short target_base_height, unsigned short target_levels) const {
  auto & fd = getImageFormat(format);
  assert(!fd.getCompression());
  size_t target_size = calculateOffset(target_base_width, target_base_height, target_levels, format);
  // cerr << "scaling to " << target_base_width << " " << target_base_height << " " << target_levels << " => " << target_size << " bytes\n";
  auto output_data = std::make_unique<unsigned char[]>(target_size);
  unsigned short target_width = target_base_width, target_height = target_base_height;
  stbir_resize_uint8(data_.get(), getWidth(), getHeight(), 0, output_data_.get(), target_width, target_height, 0, fd.getBytesPerPixel());

  if (target_levels > 1) {
    // cerr << "creating mipmaps for format, channels = " << fd.getNumChannels() << ", bpp = " << fd.getBytesPerPixel() << endl;

    unsigned short source_width = target_width, source_height = target_height;
    target_width /= 2;
    target_height /= 2;

    unsigned short nch = fd.getBytesPerPixel();
    
    for (unsigned int level = 1; level < target_levels; level++) {
      unsigned char * source_data = output_data_.get() + calculateOffset(target_base_width, target_base_height, level - 1, format);
      unsigned char * target_data = output_data_.get() + calculateOffset(target_base_width, target_base_height, level, format);
      for (unsigned int y = 0; y < target_height; y++) {               
	for (unsigned int x = 0; x < target_width; x++) {
	  unsigned int source_offset = (2 * y * source_width + 2 * x) * nch;
	  unsigned int target_offset = (y * target_width + x) * nch;
	  for (unsigned int c = 0; c < nch; c++) {
	    target_data[target_offset] = ((unsigned int)source_data[source_offset]  + 
					  (unsigned int)source_data[source_offset + nch]  + 
					  (unsigned int)source_data[source_offset + nch + nch * source_width] +
					  (unsigned int)source_data[source_offset + nch * source_width]) / 4; 
	    source_offset++;
	    target_offset++;
	  }
	}
      }
      source_width = target_width;
      source_height = target_height;
      target_width /= 2;
      target_height /= 2;
    }
  }
  return make_unique<ImageData>(output_data_.get(), getInternalFormat(), target_base_width, target_base_height, target_levels);
}
#endif
