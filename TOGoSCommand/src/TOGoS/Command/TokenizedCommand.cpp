#include <TOGoSStringView.h>
#include "./ParseResult.h"
#include "./TokenizedCommand.h"
#include "./tokenize.h"

using StringView = TOGoS::StringView;
using ParseError = TOGoS::Command::ParseError;
using TokenizedCommand = TOGoS::Command::TokenizedCommand;
using TokenizedCommandParseResult = TOGoS::Command::ParseResult<TokenizedCommand>;

TokenizedCommand::TokenizedCommand(const StringView& path, const StringView& argStr, const std::vector<StringView> args) : path(path), argStr(argStr), args(args) { }
TokenizedCommand::TokenizedCommand() = default;

static bool isWhitespace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
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
