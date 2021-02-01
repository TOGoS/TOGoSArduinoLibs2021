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
#include <TOGoS/Command/ParseError.h>
#include <TOGoS/Command/ParseResult.h>
#include <TOGoS/Command/TokenizedCommand.h>

using StringView = TOGoS::StringView;
using TokenizedCommand = TOGoS::Command::TokenizedCommand;
using ParseError = TOGoS::Command::ParseError;
using TCPR = TOGoS::Command::ParseResult<TokenizedCommand>;

Print& operator<<(Print& printer, const TokenizedCommand &cmd) {
  printer << "path:{" << cmd.path << "}";

  if( cmd.argStr.size() > 0 ) {
    printer << " argStr:{" << cmd.argStr << "}";
  }
  printer << " args:{";
  for( int i=0; i<cmd.args.size(); ++i ) {
    if( i > 0 ) printer << " ";
    printer << "{" << cmd.args[i] << "}";
  }
  printer << "}";
  return printer;
}


Print& operator<<(Print& printer, const ParseError& parseError) {
  return printer << parseError.getErrorMessage() << " at index " << parseError.errorOffset;
}

void processLine(const StringView& line) {
  TCPR pr = TokenizedCommand::parse(line);
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
