#ifndef _TOGOS_COMMAND_PARSEERROR_H
#define _TOGOS_COMMAND_PARSEERROR_H

namespace TOGoS { namespace Command {
  struct ParseError {
    enum class ErrorCode {
      NO_ERROR,
      UNEXPECTED_BRACE,
      UNEXPECTED_EOI,
    } errorCode;
    size_t errorOffset;
    
    const char *getErrorMessage() const {
      switch( errorCode ) {
      case ErrorCode::NO_ERROR: return "no error";
      case ErrorCode::UNEXPECTED_BRACE: return "invalid brace";
      case ErrorCode::UNEXPECTED_EOI: return "invalid end of input";
      default: return "unknown parse error code";
      }
    }
    
    ParseError(ErrorCode code, size_t offset) : errorCode(code), errorOffset(offset) {}
    ParseError() : errorCode(ErrorCode::NO_ERROR), errorOffset(0) {}
  };
}}

#endif
