#ifndef _CANVAS_FILENAMECONVERTER_H_
#define _CANVAS_FILENAMECONVERTER_H_

namespace canvas {
  class FilenameConverter {
  public:
    virtual ~FilenameConverter() = default;
    virtual bool convert(const std::string & input, std::string & output) = 0;
  };
};
#endif
