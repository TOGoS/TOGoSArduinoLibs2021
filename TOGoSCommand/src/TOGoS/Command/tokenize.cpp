#include <TOGoSStringView.h>
#include "./tokenize.h"
#include "./ParseError.h"
#include "./ParseResult.h"

using StringView = TOGoS::StringView;
using ParseError = TOGoS::Command::ParseError;
using TokenList = std::vector<TOGoS::StringView>;
using TokenListParseResult = TOGoS::Command::ParseResult<std::vector<TOGoS::StringView>>;

static bool isWhitespace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

TokenListParseResult TOGoS::Command::tokenize(const StringView& readFrom, size_t index0) {
  const char *cur = readFrom.begin();
  const char *end = readFrom.end();
  TokenList arge;

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
        return TokenListParseResult::errored(ParseError::ErrorCode::UNEXPECTED_BRACE, index0 + (cur - readFrom.begin()));
      case '}':
        return TokenListParseResult::errored(ParseError::ErrorCode::UNEXPECTED_BRACE, index0 + (cur - readFrom.begin()));
      }
      break;
    case ParseState::BRACED:
      if( cur == end ) return TokenListParseResult::errored(ParseError::ErrorCode::UNEXPECTED_EOI, index0 + (cur - readFrom.begin()));
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

  return TokenListParseResult::ok(std::move(arge));
}

