#ifndef _TOGOS_COMMAND_TOKENIZEDCOMMAND_H
#define _TOGOS_COMMAND_TOKENIZEDCOMMAND_H

#include <TOGoSStringView.h>
#include "./ParseError.h"
#include "./ParseResult.h"

namespace TOGoS { namespace Command {
  using TokenList = std::vector<TOGoS::StringView>;

  class TokenizedCommand {
  public:
    const TOGoS::StringView path;
    const TOGoS::StringView argStr;
    const TokenList args;

    TokenizedCommand(const TOGoS::StringView& path, const TOGoS::StringView& argStr, const std::vector<TOGoS::StringView> args)
    : path(path), argStr(argStr), args(args) { }
    TokenizedCommand() {}

    bool isEmpty() const {
      return this->path.size() == 0;
    }

    static TokenizedCommand empty() {
      //return TokenizedCommand(TOGoS::StringView(), TOGoS::StringView(), std::vector<TOGoS::StringView>());
      return TokenizedCommand();
    }
    static ParseResult<TokenizedCommand> parse(const TOGoS::StringView &input);
  };
    
  static bool isWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  }

  ParseResult<TokenList> parseList(const TOGoS::StringView& readFrom, size_t index0) {
    const char *cur = readFrom.begin();
    const char *end = readFrom.end();
    TokenList arge;

    using Result = ParseResult<TokenList>;
  
    enum class ParseState {
      BETWEEN,
      BARE,
      BRACED,
    } state = ParseState::BETWEEN;
    const char *currentWordBegin = 0; // If null, no 'word begin' has been found
    int braceDepth = 0;
  
    while( true ) {
      switch( state ) {
      case ParseState::BETWEEN:
        if( cur == end ) goto end_of_input;
        if( isWhitespace(*cur) ) break;
        switch( *cur ) {
        case '#':
          goto end_of_input;
        case '{':
          braceDepth = 1;
          state = ParseState::BRACED;
          currentWordBegin = cur+1;
          break;
        default:
          state = ParseState::BARE;
          currentWordBegin = cur;
          break;
        }
        break;
      case ParseState::BARE:
        if( cur == end ) {
          arge.emplace_back(currentWordBegin, cur - currentWordBegin);
          goto end_of_input;
        }
        if( isWhitespace(*cur) ) {
          arge.emplace_back(currentWordBegin, cur - currentWordBegin);
          state = ParseState::BETWEEN;
          break;
        }
        switch( *cur ) {
        case '{':
          return Result::errored(ParseError::ErrorCode::UNEXPECTED_BRACE, index0 + (cur - readFrom.begin()));
        case '}':
          return Result::errored(ParseError::ErrorCode::UNEXPECTED_BRACE, index0 + (cur - readFrom.begin()));
        }
        break;
      case ParseState::BRACED:
        if( cur == end ) return Result::errored(ParseError::ErrorCode::UNEXPECTED_EOI, index0 + (cur - readFrom.begin()));
        switch( *cur ) {
        case '{':
          ++braceDepth;
          break;
        case '}':
          --braceDepth;
          break;
        }
        if( braceDepth == 0 ) {
          arge.emplace_back(currentWordBegin, cur - currentWordBegin);
          state = ParseState::BETWEEN;
        }
        break;
      }
      ++cur;
    }
  end_of_input:

    return Result::ok(std::move(arge));
  }
}}

#endif
