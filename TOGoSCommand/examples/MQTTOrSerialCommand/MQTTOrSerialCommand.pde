/**
 * Will (when done) demonstrate using TOGoS::TLIBuffer to read commands from Serial
 * and also from MQTT!
 */

#include <TOGoSCommand.h>
#include <TOGoSStreamOperators.h>

#include <ESP8266WiFi.h>
#include <vector>

class TokenizedCommand {
public:
  const TOGoS::StringView path;
  const TOGoS::StringView argStr;
  const std::vector<TOGoS::StringView> args;

  TokenizedCommand(const TOGoS::StringView& path, const TOGoS::StringView& argStr, const std::vector<TOGoS::StringView> args)
    : path(path), argStr(argStr), args(args) { }

  bool isEmpty() const {
    return this->path.size() == 0;
  }

  static TokenizedCommand empty() {
    return TokenizedCommand(TOGoS::StringView(), TOGoS::StringView(), std::vector<TOGoS::StringView>());
  }
};

bool isWhitespace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

TokenizedCommand parseCommandLine(const TOGoS::StringView& line) {
  size_t i = 0;

  // Skip whitespace
  while( i < line.size() && isWhitespace(line[i]) ) ++i;
  if( i == line.size() || line[i] == '#' ) return TokenizedCommand::empty();

  size_t pathBegin = i;
  while( i < line.size() && !isWhitespace(line[i]) ) ++i;
  TOGoS::StringView path(&line[pathBegin], i-pathBegin);

  return TokenizedCommand(path, TOGoS::StringView(), std::vector<TOGoS::StringView>()); // TODO parse args
}

void processLine(const TOGoS::StringView& line) {
  TokenizedCommand tcmd = parseCommandLine(line);
  
  if( tcmd.path.size() == 0 ) return;
  
  if( tcmd.path == "hi" || tcmd.path == "hello" ) {
    Serial << "# Hi there!\n";
  } else if( tcmd.path == "bye" ) {
    Serial << "# See ya later!\n";
  } else {
    Serial << "# Read command: " << tcmd.path << "\n";
  }
}

TOGoS::TLIBuffer commandBuffer;

void setup() {
  Serial.begin(115200);
}

void loop() {
  while( Serial.available() > 0 ) {
    TOGoS::TLIBuffer::BufferState bufState = commandBuffer.onChar(Serial.read());
    if( bufState == TOGoS::TLIBuffer::BufferState::READY ) {
      processLine( commandBuffer.str() );
      commandBuffer.reset();
    }
  }
  delay(10);
}
