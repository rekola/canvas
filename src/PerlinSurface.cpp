#include <PerlinSurface.h>

#include <Context.h>
#include <Image.h>

#include <algorithm>
#include <cmath>

using namespace std;
using namespace canvas;

// Ken Perlin's reference permutation.
static unsigned char permutation[] = { 151,160,137,91,90,15,
				       131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
				       190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
				       88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
				       77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
				       102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
				       135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
				       5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
				       223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
				       129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
				       251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
				       49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
				       138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

static inline float fade(float t) {
  return t * t * t * (t * (t * 6 - 15) + 10);
}

static inline float grad(int hashval, float x, float y, float z) {
  // Convert the low 4 bits of the hash code into 12 gradient directions.
  int h = hashval & 15;
  float u = h < 8 ? x : y;
  float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
  return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

static inline float lerp(float t, float a, float b) {
  return a + t * (b - a);
}

PerlinSurface::PerlinSurface(unsigned int _logical_width, unsigned int _logical_height, unsigned int _actual_width, unsigned int _actual_height, int _octaves, float _alpha, float _beta, float _period)
  : Surface(_logical_width, _logical_height, _actual_width, _actual_height, 4),
    octaves(std::max(1, _octaves)), alpha(_alpha), beta(_beta), period(_period > 0.0f ? _period : 1.0f)
{
  for (int i = 0; i < 256; i++) {
    p[256 + i] = p[i] = permutation[i];
  }
}

float
PerlinSurface::evaluatePerlin(float x, float y, float z) {
  // Find the unit cube that contains the point.
  int flx = (int)floorf(x), fly = (int)floorf(y), flz = (int)floorf(z);
  int X = flx & 255, Y = fly & 255, Z = flz & 255;
  // Find relative x, y, z of point in cube.
  x -= flx;
  y -= fly;
  z -= flz;
  // Compute fade curves for each of x, y, z.
  float u = fade(x), v = fade(y), w = fade(z);
  // Hash coordinates of the 8 cube corners.
  int A = p[X    ] + Y, AA = p[A] + Z, AB = p[A + 1] + Z;
  int B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;
  // And add blended results from 8 corners of the cube.
  return lerp(w, lerp(v, lerp(u, grad(p[AA  ], x,   y,   z  ),
			         grad(p[BA  ], x-1, y,   z  )),
		         lerp(u, grad(p[AB  ], x,   y-1, z  ),
			         grad(p[BB  ], x-1, y-1, z  ))),
	         lerp(v, lerp(u, grad(p[AA+1], x,   y,   z-1),
			         grad(p[BA+1], x-1, y,   z-1)),
		         lerp(u, grad(p[AB+1], x,   y-1, z-1),
			         grad(p[BB+1], x-1, y-1, z-1))));
}

float
PerlinSurface::evaluate(float x, float y, float z) {
  float scale = 1;
  float v = 0;
  for (int o = 0; o < octaves; o++) {
    v += evaluatePerlin(x, y, z) / scale;
    x *= beta;
    y *= beta;
    z *= beta;
    scale *= alpha;
  }
  return v;
}

void *
PerlinSurface::lockMemory(bool write_access) {
  size_t w = getActualWidth(), h = getActualHeight();
  delete[] buffer;
  buffer = new unsigned char[w * h * 4];
  // The layers are independent, so their spread adds in quadrature: divide
  // by that, so more octaves add detail without clipping or losing contrast.
  float total = 0, weight = 1;
  for (int o = 0; o < octaves; o++) {
    total += weight * weight;
    weight /= alpha;
  }
  total = sqrtf(total);
  size_t offset = 0;
  for (size_t y = 0; y < h; y++) {
    for (size_t x = 0; x < w; x++) {
      // The pixel centre, in noise cells: the noise is zero on whole numbers.
      float n = evaluate((x + 0.5f) / period, (y + 0.5f) / period, 0) / total;
      int f = int((n + 0.7f) / 1.4f * 255);
      unsigned char v = f < 0 ? 0 : (f >= 255 ? 255 : (unsigned char)f);
      buffer[offset++] = v;
      buffer[offset++] = v;
      buffer[offset++] = v;
      buffer[offset++] = 255;
    }
  }
  return buffer;
}

std::unique_ptr<Image>
PerlinSurface::createImage(float display_scale) {
  auto image = std::make_unique<Image>(display_scale);
  image->getDataPtr() = readPixels();
  return image;
}
