/**
 * Will (when done) demonstrate using TOGoS::TLIBuffer to read commands from Serial
 * and also from MQTT!
 */

#include <TOGoSStringView.h>
#include <TOGoSCommand.h>
#include <TOGoS/Command/TLIBuffer.h>
#include <TOGoSStreamOperators.h>
#include <Print.h>
#include <ESP8266WiFi.h>
#include <vector>

class TokenizedCommand {
public:
  const TOGoS::StringView path;
  const TOGoS::StringView argStr;
  const std::vector<TOGoS::StringView> args;

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
};

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
  ParseError() : errorCode(ErrorCode::NO_ERROR), errorOffset(0) {};
};

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

using TokenList = std::vector<TOGoS::StringView>;

Print& operator<<(Print& printer, const ParseError& parseError) {
  return printer << parseError.getErrorMessage() << " at index " << parseError.errorOffset;
}

Print& operator<<(Print& printer, const TokenizedCommand &cmd) {
  printer << "path:" << cmd.path;
  
  if( cmd.argStr.size() > 0 ) {
    printer << " cmdArgs:" << cmd.argStr;
  }
  printer << " args:{";
  for( int i=0; i<cmd.args.size(); ++i ) {
    if( i > 0 ) printer << " ";
    printer << "{" << cmd.args[i] << "}";
  }
  printer << "}";
  return printer;
}

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

ParseResult<TokenizedCommand> parseCommandLine(const TOGoS::StringView& line) {
  size_t i = 0;

  // TODO: Just parse the whole thing as one big list;
  // the command path doesn't need to be treated special.

  // Skip whitespace
  while( i < line.size() && isWhitespace(line[i]) ) ++i;
  if( i == line.size() || line[i] == '#' ) {
    return ParseResult<TokenizedCommand>::ok(std::move(TokenizedCommand::empty()));
  }

  size_t pathBegin = i;
  while( i < line.size() && !isWhitespace(line[i]) ) ++i;
  TOGoS::StringView path(&line[pathBegin], i-pathBegin);

  // Skip whitespace until arguments string
  while( i < line.size() && isWhitespace(line[i]) ) ++i;
  TOGoS::StringView argStr(&path[i], line.size()-i);
  
  auto listParseResult = parseList(argStr, i);
  if( listParseResult.isError() ) {
    return ParseResult<TokenizedCommand>::errored(listParseResult.error);
  } else {
    return ParseResult<TokenizedCommand>::ok(TokenizedCommand(path, argStr, listParseResult.value));
  }
}

void processLine(const TOGoS::StringView& line) {
  ParseResult<TokenizedCommand> pr = parseCommandLine(line);
  if( pr.isError() ) {
    Serial << "# Error parsing command: " << pr.error << "\n";
    return;
  }

  const TokenizedCommand &tcmd = pr.value;

  if( tcmd.path.size() == 0 ) return;

  if( tcmd.path == "echo" ) {
    for( int i=0; i<tcmd.args.size(); ++i ) {
      if( i > 0 ) Serial << " ";
      Serial << tcmd.args[i];
    }
    Serial << "\n";
  } else if( tcmd.path == "echo/lines" ) {
    for( auto arg : tcmd.args ) {
      Serial << arg << "\n";
    }
  } else if( tcmd.path == "hi" || tcmd.path == "hello" ) {
    Serial << "# Hi there!\n";
  } else if( tcmd.path == "bye" ) {
    Serial << "# See ya later!\n";
  } else {
    Serial << "# Unknown command: " << tcmd << "\n";
  }
}

using TLIBuffer = TOGoS::Command::TLIBuffer;

TLIBuffer commandBuffer;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial << "# Hi, I'm MQTTOrSerialCommand!\n";
  Serial << "# Enter commands of the form:\n";
  Serial << "#   <path> [<argument> ...]\n";
  Serial << "# For example:\n";
  Serial << "#   echo/lines foo bar {baz quux}\n";
}

void loop() {
  while( Serial.available() > 0 ) {
    TLIBuffer::BufferState bufState = commandBuffer.onChar(Serial.read());
    if( bufState == TLIBuffer::BufferState::READY ) {
      processLine( commandBuffer.str() );
      commandBuffer.reset();
    }
  }
  delay(10);
}
