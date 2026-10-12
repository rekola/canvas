#ifndef _CANVAS_FILENAMECONVERTER_H_
#define _CANVAS_FILENAMECONVERTER_H_

#include <string>

namespace canvas {
  // Turns the name an image was requested by into the path to load it from
  // (an app bundle's resource path, an install directory). Returns false if
  // it has no path for the name, which is then used as given.
  class FilenameConverter {
  public:
    virtual ~FilenameConverter() = default;
    virtual bool convert(const std::string & input, std::string & output) = 0;
  };
};
#endif
