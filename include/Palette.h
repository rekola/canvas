#ifndef _CANVAS_PALETTE_H_
#define _CANVAS_PALETTE_H_

#include <cstddef>
#include <vector>

namespace canvas {
  // A colour table of up to 256 opaque entries: what a 256-colour terminal
  // or a Sixel image can address. The index of an entry is the code the
  // output uses.
  class Palette {
  public:
    struct Entry {
      unsigned char red, green, blue;
    };

    Palette() { }
    // At most 256 entries; any more are ignored.
    explicit Palette(std::vector<Entry> _entries);

    // The xterm 256-colour palette, indexed by xterm colour number: 0-15 the
    // system colours (xterm's defaults; terminals let users change them),
    // 16-231 the 6x6x6 cube with levels 0, 95, 135, 175, 215, 255, and
    // 232-255 a grey ramp from 8 to 238.
    static Palette xterm256();

    size_t size() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }
    const Entry & operator[](size_t i) const { return entries_[i]; }

    // The entry closest to the colour (squared distance in RGB), searching
    // from index first on, so the xterm system colours can be left out with
    // first = 16. The lowest index wins a tie. An empty palette gives 0.
    unsigned int nearest(int red, int green, int blue, size_t first = 0) const;

    // The usual distance between neighbouring entries along a colour channel
    // (the median over the entries of the distance to the nearest other
    // entry), 1 to 255. Ordered dithering spreads its threshold over it.
    int spacing() const;

  private:
    std::vector<Entry> entries_;
    mutable int spacing_ = 0;
  };
};

#endif
