#ifndef _TOGOS_STRINGVIEW_H
#define _TOGOS_STRINGVIEW_H

#include <cstring>
#include <stddef.h>
#include <string>

namespace TOGoS {
  /** Intended as a drop-in replacement for std::string_view */
  struct StringView {
    const char *_begin;
    size_t _size;
    StringView() : _begin(nullptr), _size(0) {}
    StringView(const char *str) : _begin(str), _size(strlen(str)) {}
    StringView(const char *str, size_t len) : _begin(str), _size(len) {}
    StringView(const std::string& str) : _begin(str.data()), _size(str.size()) {}
    const char *begin() const { return this->_begin; }
    const char *end() const { return this->_begin + this->_size; }
    const char &operator[](size_t offset) const { return this->_begin[offset]; }
    size_t size() const { return this->_size; }
    operator std::string() const { return std::string(this->begin(), this->size()); }
    bool operator==(const StringView &other) const {
      if( other.size() != this->_size ) return false;
      return memcmp(this->begin(), other.begin(), this->_size) == 0;
    }
  };
}

#endif
