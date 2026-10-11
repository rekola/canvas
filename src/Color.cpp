#include <Color.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace canvas;

Color Color::BLACK(0.0f, 0.0f, 0.0f, 1.0f);
Color Color::WHITE(1.0f, 1.0f, 1.0f, 1.0f);
Color Color::RED(1.0f, 0.0f, 0.0f, 1.0f);

namespace {
  struct NamedColor { const char * name; unsigned int rgb; };

  // The CSS named colours, sorted by name.
  const NamedColor named_colors[] = {
  {"aliceblue", 0xf0f8ff},
  {"antiquewhite", 0xfaebd7},
  {"aqua", 0x00ffff},
  {"aquamarine", 0x7fffd4},
  {"azure", 0xf0ffff},
  {"beige", 0xf5f5dc},
  {"bisque", 0xffe4c4},
  {"black", 0x000000},
  {"blanchedalmond", 0xffebcd},
  {"blue", 0x0000ff},
  {"blueviolet", 0x8a2be2},
  {"brown", 0xa52a2a},
  {"burlywood", 0xdeb887},
  {"cadetblue", 0x5f9ea0},
  {"chartreuse", 0x7fff00},
  {"chocolate", 0xd2691e},
  {"coral", 0xff7f50},
  {"cornflowerblue", 0x6495ed},
  {"cornsilk", 0xfff8dc},
  {"crimson", 0xdc143c},
  {"cyan", 0x00ffff},
  {"darkblue", 0x00008b},
  {"darkcyan", 0x008b8b},
  {"darkgoldenrod", 0xb8860b},
  {"darkgray", 0xa9a9a9},
  {"darkgreen", 0x006400},
  {"darkgrey", 0xa9a9a9},
  {"darkkhaki", 0xbdb76b},
  {"darkmagenta", 0x8b008b},
  {"darkolivegreen", 0x556b2f},
  {"darkorange", 0xff8c00},
  {"darkorchid", 0x9932cc},
  {"darkred", 0x8b0000},
  {"darksalmon", 0xe9967a},
  {"darkseagreen", 0x8fbc8f},
  {"darkslateblue", 0x483d8b},
  {"darkslategray", 0x2f4f4f},
  {"darkslategrey", 0x2f4f4f},
  {"darkturquoise", 0x00ced1},
  {"darkviolet", 0x9400d3},
  {"deeppink", 0xff1493},
  {"deepskyblue", 0x00bfff},
  {"dimgray", 0x696969},
  {"dimgrey", 0x696969},
  {"dodgerblue", 0x1e90ff},
  {"firebrick", 0xb22222},
  {"floralwhite", 0xfffaf0},
  {"forestgreen", 0x228b22},
  {"fuchsia", 0xff00ff},
  {"gainsboro", 0xdcdcdc},
  {"ghostwhite", 0xf8f8ff},
  {"gold", 0xffd700},
  {"goldenrod", 0xdaa520},
  {"gray", 0x808080},
  {"green", 0x008000},
  {"greenyellow", 0xadff2f},
  {"grey", 0x808080},
  {"honeydew", 0xf0fff0},
  {"hotpink", 0xff69b4},
  {"indianred", 0xcd5c5c},
  {"indigo", 0x4b0082},
  {"ivory", 0xfffff0},
  {"khaki", 0xf0e68c},
  {"lavender", 0xe6e6fa},
  {"lavenderblush", 0xfff0f5},
  {"lawngreen", 0x7cfc00},
  {"lemonchiffon", 0xfffacd},
  {"lightblue", 0xadd8e6},
  {"lightcoral", 0xf08080},
  {"lightcyan", 0xe0ffff},
  {"lightgoldenrodyellow", 0xfafad2},
  {"lightgray", 0xd3d3d3},
  {"lightgreen", 0x90ee90},
  {"lightgrey", 0xd3d3d3},
  {"lightpink", 0xffb6c1},
  {"lightsalmon", 0xffa07a},
  {"lightseagreen", 0x20b2aa},
  {"lightskyblue", 0x87cefa},
  {"lightslategray", 0x778899},
  {"lightslategrey", 0x778899},
  {"lightsteelblue", 0xb0c4de},
  {"lightyellow", 0xffffe0},
  {"lime", 0x00ff00},
  {"limegreen", 0x32cd32},
  {"linen", 0xfaf0e6},
  {"magenta", 0xff00ff},
  {"maroon", 0x800000},
  {"mediumaquamarine", 0x66cdaa},
  {"mediumblue", 0x0000cd},
  {"mediumorchid", 0xba55d3},
  {"mediumpurple", 0x9370db},
  {"mediumseagreen", 0x3cb371},
  {"mediumslateblue", 0x7b68ee},
  {"mediumspringgreen", 0x00fa9a},
  {"mediumturquoise", 0x48d1cc},
  {"mediumvioletred", 0xc71585},
  {"midnightblue", 0x191970},
  {"mintcream", 0xf5fffa},
  {"mistyrose", 0xffe4e1},
  {"moccasin", 0xffe4b5},
  {"navajowhite", 0xffdead},
  {"navy", 0x000080},
  {"oldlace", 0xfdf5e6},
  {"olive", 0x808000},
  {"olivedrab", 0x6b8e23},
  {"orange", 0xffa500},
  {"orangered", 0xff4500},
  {"orchid", 0xda70d6},
  {"palegoldenrod", 0xeee8aa},
  {"palegreen", 0x98fb98},
  {"paleturquoise", 0xafeeee},
  {"palevioletred", 0xdb7093},
  {"papayawhip", 0xffefd5},
  {"peachpuff", 0xffdab9},
  {"peru", 0xcd853f},
  {"pink", 0xffc0cb},
  {"plum", 0xdda0dd},
  {"powderblue", 0xb0e0e6},
  {"purple", 0x800080},
  {"rebeccapurple", 0x663399},
  {"red", 0xff0000},
  {"rosybrown", 0xbc8f8f},
  {"royalblue", 0x4169e1},
  {"saddlebrown", 0x8b4513},
  {"salmon", 0xfa8072},
  {"sandybrown", 0xf4a460},
  {"seagreen", 0x2e8b57},
  {"seashell", 0xfff5ee},
  {"sienna", 0xa0522d},
  {"silver", 0xc0c0c0},
  {"skyblue", 0x87ceeb},
  {"slateblue", 0x6a5acd},
  {"slategray", 0x708090},
  {"slategrey", 0x708090},
  {"snow", 0xfffafa},
  {"springgreen", 0x00ff7f},
  {"steelblue", 0x4682b4},
  {"tan", 0xd2b48c},
  {"teal", 0x008080},
  {"thistle", 0xd8bfd8},
  {"tomato", 0xff6347},
  {"turquoise", 0x40e0d0},
  {"violet", 0xee82ee},
  {"wheat", 0xf5deb3},
  {"white", 0xffffff},
  {"whitesmoke", 0xf5f5f5},
  {"yellow", 0xffff00},
  {"yellowgreen", 0x9acd32},
  };

  int get_xdigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
  }

  bool set_hex(Color & c, const std::string & h) {
    if (h.size() != 3 && h.size() != 4 && h.size() != 6 && h.size() != 8) return false;
    int v[8];
    for (size_t i = 0; i < h.size(); i++) {
      if ((v[i] = get_xdigit(h[i])) < 0) return false;
    }
    float channel[4] = { 0, 0, 0, 255 };
    if (h.size() <= 4) {
      for (size_t i = 0; i < h.size(); i++) channel[i] = float(v[i] * 17);
    } else {
      for (size_t i = 0; i < h.size() / 2; i++) channel[i] = float(v[2 * i] * 16 + v[2 * i + 1]);
    }
    c = Color(channel[0] / 255.0f, channel[1] / 255.0f, channel[2] / 255.0f, channel[3] / 255.0f);
    return true;
  }

  // The numbers of rgb(...)/rgba(...), separated by commas, spaces or a
  // slash before the alpha. A percentage is a fraction of full scale.
  bool parse_function(const std::string & args, Color & c) {
    std::vector<float> v;
    std::vector<bool> pct;
    const char * p = args.c_str();
    while (*p) {
      while (*p == ',' || *p == '/' || std::isspace((unsigned char)*p)) p++;
      if (!*p) break;
      char * end;
      float f = std::strtof(p, &end);
      if (end == p) return false;
      p = end;
      bool is_pct = *p == '%';
      if (is_pct) p++;
      v.push_back(f);
      pct.push_back(is_pct);
    }
    if (v.size() < 3 || v.size() > 4) return false;
    float out[4] = { 0, 0, 0, 1 };
    for (size_t i = 0; i < v.size(); i++) {
      float f = pct[i] ? v[i] / 100.0f : (i < 3 ? v[i] / 255.0f : v[i]);
      out[i] = std::min(1.0f, std::max(0.0f, f));
    }
    c = Color(out[0], out[1], out[2], out[3]);
    return true;
  }
}

Color &
Color::operator=(const std::string & s) {
  setValue(s);
  return *this;
}

// Parses #rgb, #rgba, #rrggbb, #rrggbbaa, rgb(), rgba() and the CSS colour
// names (and "transparent"). Anything else is opaque black.
void
Color::setValue(const std::string & s0) {
  size_t first = 0, last = s0.size();
  while (first < last && std::isspace((unsigned char)s0[first])) first++;
  while (last > first && std::isspace((unsigned char)s0[last - 1])) last--;
  std::string s = s0.substr(first, last - first);
  for (auto & ch : s) ch = (char)std::tolower((unsigned char)ch);

  Color c(0.0f, 0.0f, 0.0f, 1.0f);
  bool ok = false;
  if (s == "transparent") {
    c = Color(0.0f, 0.0f, 0.0f, 0.0f);
    ok = true;
  } else if (!s.empty() && s[0] == '#') {
    ok = set_hex(c, s.substr(1));
  } else if ((s.compare(0, 4, "rgb(") == 0 || s.compare(0, 5, "rgba(") == 0) && s.back() == ')') {
    size_t open = s.find('(');
    ok = parse_function(s.substr(open + 1, s.size() - open - 2), c);
  } else {
    const NamedColor * end = named_colors + sizeof(named_colors) / sizeof(named_colors[0]);
    auto it = std::lower_bound(named_colors, end, s, [](const NamedColor & n, const std::string & name) { return std::strcmp(n.name, name.c_str()) < 0; });
    if (it != end && s == it->name) {
      c = Color(((it->rgb >> 16) & 0xff) / 255.0f, ((it->rgb >> 8) & 0xff) / 255.0f, (it->rgb & 0xff) / 255.0f, 1.0f);
      ok = true;
    } else {
      // A hex colour without the '#', as this used to accept.
      ok = set_hex(c, s);
    }
  }
  if (!ok) c = Color(0.0f, 0.0f, 0.0f, 1.0f);
  red = c.red;
  green = c.green;
  blue = c.blue;
  alpha = c.alpha;
}
