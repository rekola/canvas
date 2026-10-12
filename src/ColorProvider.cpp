#include <canvas/ColorProvider.h>

using namespace std;
using namespace canvas;

ColorProvider::ColorProvider(ColorStyle _style) : style(_style)
{
  if (style == COLD) {
    colors.push_back(canvas::Color("#FFFFFF"));
    colors.push_back(canvas::Color("#EEFFFF"));
    colors.push_back(canvas::Color("#F2F2F2"));
    colors.push_back(canvas::Color("#f0f0ee"));
    colors.push_back(canvas::Color("#FFE3CC"));
    colors.push_back(canvas::Color("#F2E5DA"));
    colors.push_back(canvas::Color("#EAE6E3"));
    colors.push_back(canvas::Color("#DDDAD7"));
    colors.push_back(canvas::Color("#FFC899"));
    colors.push_back(canvas::Color("#DDCCBB"));
    colors.push_back(canvas::Color("#DDCCAA"));
    colors.push_back(canvas::Color("#D8C1AD"));
    colors.push_back(canvas::Color("#F2B179"));
    colors.push_back(canvas::Color("#AA9988"));
    colors.push_back(canvas::Color("#776655"));
    colors.push_back(canvas::Color("#995522"));
    colors.push_back(canvas::Color("#7F5D3F"));
    colors.push_back(canvas::Color("#A84D00"));
    colors.push_back(canvas::Color("#884411"));
    colors.push_back(canvas::Color("#4C443D"));
    colors.push_back(canvas::Color("#663300"));
    colors.push_back(canvas::Color("#43270F"));
    colors.push_back(canvas::Color("#2B2B2B"));
    colors.push_back(canvas::Color("#221100"));
    colors.push_back(canvas::Color("#111110"));
  } else if (style == CHART) {
    colors.push_back(canvas::Color("#96b400"));
    colors.push_back(canvas::Color("#f55a28"));
		     
    colors.push_back(canvas::Color("#3fdeff"));
    colors.push_back(canvas::Color("#00899a"));
    colors.push_back(canvas::Color("#298c13"));
    colors.push_back(canvas::Color("#b9ce6b"));
    colors.push_back(canvas::Color("#bf9d41"));
    colors.push_back(canvas::Color("#b54848"));
    colors.push_back(canvas::Color("#f74ca2"));
  } else if (style == CHART2) {
    colors.push_back(canvas::Color("#006b98"));
    colors.push_back(canvas::Color("#499d2e"));
    colors.push_back(canvas::Color("#eb6395"));
    colors.push_back(canvas::Color("#f7954a"));
    colors.push_back(canvas::Color("#ff573d"));
  }
}
