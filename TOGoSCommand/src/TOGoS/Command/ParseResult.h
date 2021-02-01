#ifndef _TOGOS_COMMAND_PARSERESULT_H
#define _TOGOS_COMMAND_PARSERESULT_H

#include "./ParseError.h"

namespace TOGoS { namespace Command {
  template<typename T>
  class ParseResult {
  public:
    T value;
    ParseError error;
  protected:  
    ParseResult(const T& value) : value(value) { }
    ParseResult(T&& value) : value(std::move(value)) { }
    ParseResult(ParseError error) : value(), error(error) { }
  public:
    bool isError() const {
      return error.errorCode != ParseError::ErrorCode::NO_ERROR;
    }
    static ParseResult errored(const ParseError& error) {
      return ParseResult(error);
    }
    static ParseResult errored(ParseError::ErrorCode code, size_t offset) {
      return ParseResult(ParseError(code, offset));
    }
    static ParseResult ok(const T &value) {
      return ParseResult(value);
    }
    static ParseResult ok(T &&value) {
      return ParseResult(std::move(value));
    }
  };
}}

#endif

