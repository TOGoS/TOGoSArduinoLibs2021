#include <TOGoSStringView.h>
#include "./ParseResult.h"
#include "./TokenizedCommand.h"

using StringView = TOGoS::StringView;
using ParseError = TOGoS::Command::ParseError;
using TokenList = TOGoS::Command::TokenList;
using TokenListParseResult = TOGoS::Command::ParseResult<TokenList>;
using TokenizedCommand = TOGoS::Command::TokenizedCommand;
using TokenizedCommandParseResult = TOGoS::Command::ParseResult<TokenizedCommand>;

TokenizedCommand::TokenizedCommand(const StringView& path, const StringView& argStr, const std::vector<StringView> args) : path(path), argStr(argStr), args(args) { }
TokenizedCommand::TokenizedCommand() = default;

static bool isWhitespace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

TokenListParseResult TOGoS::Command::tokenize(const StringView& readFrom, size_t index0) {
  const char *cur = readFrom.begin();
  const char *end = readFrom.end();
  TokenList arge;

  using Result = TokenListParseResult;
  
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

TokenizedCommandParseResult TokenizedCommand::parse(const StringView& line) {
  size_t i = 0;

  // TODO: Just parse the whole thing as one big list;
  // the command path doesn't need to be treated special!

  // Skip whitespace
  while( i < line.size() && isWhitespace(line[i]) ) ++i;
  if( i == line.size() || line[i] == '#' ) {
    return TokenizedCommandParseResult::ok(std::move(TokenizedCommand::empty()));
  }

  size_t pathBegin = i;
  while( i < line.size() && !isWhitespace(line[i]) ) ++i;
  StringView path(&line[pathBegin], i-pathBegin);

  // Skip whitespace until arguments string
  while( i < line.size() && isWhitespace(line[i]) ) ++i;
  StringView argStr(&path[i], line.size()-i);
  
  auto listParseResult = tokenize(argStr, i);
  if( listParseResult.isError() ) {
    return ParseResult<TokenizedCommand>::errored(listParseResult.error);
  } else {
    return ParseResult<TokenizedCommand>::ok(TokenizedCommand(path, argStr, listParseResult.value));
  }
}
