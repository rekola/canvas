#include <Palette.h>

#include <algorithm>
#include <cstdlib>

using namespace canvas;

Palette::Palette(std::vector<Entry> _entries) : entries_(std::move(_entries)) {
  if (entries_.size() > 256) entries_.resize(256);
}

Palette
Palette::xterm256() {
  static const unsigned char system_colors[16][3] = {
    { 0x00, 0x00, 0x00 }, { 0x80, 0x00, 0x00 }, { 0x00, 0x80, 0x00 }, { 0x80, 0x80, 0x00 },
    { 0x00, 0x00, 0x80 }, { 0x80, 0x00, 0x80 }, { 0x00, 0x80, 0x80 }, { 0xc0, 0xc0, 0xc0 },
    { 0x80, 0x80, 0x80 }, { 0xff, 0x00, 0x00 }, { 0x00, 0xff, 0x00 }, { 0xff, 0xff, 0x00 },
    { 0x00, 0x00, 0xff }, { 0xff, 0x00, 0xff }, { 0x00, 0xff, 0xff }, { 0xff, 0xff, 0xff }
  };
  static const unsigned char levels[6] = { 0, 95, 135, 175, 215, 255 };
  std::vector<Entry> entries;
  for (auto & c : system_colors) entries.push_back(Entry{ c[0], c[1], c[2] });
  for (int r = 0; r < 6; r++)
    for (int g = 0; g < 6; g++)
      for (int b = 0; b < 6; b++) entries.push_back(Entry{ levels[r], levels[g], levels[b] });
  for (int i = 0; i < 24; i++) {
    unsigned char v = (unsigned char)(8 + 10 * i);
    entries.push_back(Entry{ v, v, v });
  }
  return Palette(std::move(entries));
}

unsigned int
Palette::nearest(int red, int green, int blue, size_t first) const {
  unsigned int best = (unsigned int)std::min(first, entries_.empty() ? size_t(0) : entries_.size() - 1);
  long best_distance = -1;
  for (size_t i = first; i < entries_.size(); i++) {
    long dr = red - entries_[i].red, dg = green - entries_[i].green, db = blue - entries_[i].blue;
    long distance = dr * dr + dg * dg + db * db;
    if (best_distance < 0 || distance < best_distance) {
      best_distance = distance;
      best = (unsigned int)i;
      if (distance == 0) break;
    }
  }
  return best;
}

int
Palette::spacing() const {
  if (spacing_ == 0) {
    std::vector<int> nearest_other;
    for (size_t i = 0; i < entries_.size(); i++) {
      int best = 256;
      for (size_t j = 0; j < entries_.size(); j++) {
	if (i == j) continue;
	int d = std::max({ std::abs(entries_[i].red - entries_[j].red),
			   std::abs(entries_[i].green - entries_[j].green),
			   std::abs(entries_[i].blue - entries_[j].blue) });
	if (d > 0 && d < best) best = d;  // a duplicate colour doesn't count
      }
      if (best < 256) nearest_other.push_back(best);
    }
    if (nearest_other.empty()) {
      spacing_ = 255;
    } else {
      std::sort(nearest_other.begin(), nearest_other.end());
      spacing_ = std::min(255, std::max(1, nearest_other[nearest_other.size() / 2]));
    }
  }
  return spacing_;
}
