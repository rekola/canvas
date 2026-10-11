#ifndef _CANVAS_HITREGION_H_
#define _CANVAS_HITREGION_H_

#include "Path2D.h"

#include <string>

namespace canvas {
  class HitRegion {
  public:
    HitRegion() { }
    HitRegion(std::string id)
      : id_(std::move(id)) { }
    HitRegion(std::string id, Path2D path, std::string cursor)
      : id_(std::move(id)), path_(std::move(path)), cursor_(std::move(cursor)) { }

    bool operator==(const HitRegion & other) const {
      if (id_ != other.id_) return false;
      if (cursor_ != other.cursor_) return false;
      if (title_ != other.title_) return false;
      if (url_ != other.url_) return false;
      if (popup_rows_.size() != other.popup_rows_.size()) return false;
      for (size_t i = 0; i < popup_rows_.size(); i++) {
	if (popup_rows_[i] != other.popup_rows_[i]) return false;
      }
      return true;
    }

    bool operator!=(const HitRegion & other) const { return !(*this == other); }
	      
    void setTitle(std::string title) { title_ = title; }
    const std::string & getTitle() const { return title_; }

    void setLink(std::string url) { url_ = std::move(url); }
    const std::string & getLink() const { return url_; }

    void setPath(Path2D path) { path_ = std::move(path); }
    const Path2D & getPath() const { return path_; }

    const std::string & getId() const { return id_; }

    void setCursor(std::string cursor) { cursor_ = std::move(cursor); }
    const std::string & getCursor() const { return cursor_; }

    void addRow(std::string row) { popup_rows_.push_back(std::move(row)); }

    bool hasPopup() const { return !title_.empty() || !popup_rows_.empty(); }
    const std::vector<std::string> & getRows() const { return popup_rows_; }
    
  private:
    std::string id_;
    Path2D path_;
    std::string cursor_;

    std::string title_;
    std::string url_;
    std::vector<std::string> popup_rows_;
  };
};

#endif
