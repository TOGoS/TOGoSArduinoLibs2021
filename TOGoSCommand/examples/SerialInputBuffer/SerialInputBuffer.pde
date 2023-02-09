/**
 * Demonstrates using TOGoS::Command::TLIBuffer to read commands from Serial
 * while allowing the the rest of loop() to continue running
 */

#include <TOGoSCommand.h>
#include <TOGoS/Command/TLIBuffer.h>
#include <TOGoSStreamOperators.h>

void setup() {
  Serial.begin(115200);
}

TOGoS::Command::TLIBuffer commandBuffer;

void processLine(const TOGoS::StringView& line) {
  if( line.size() == 0 ) return;
  if( line[0] == '#' ) return;

  if( line == "hi" || line == "hello" ) {
    Serial << "# Hi there!\n";
  } else if( line == "byeo" ) {
    Serial << "# See ya later!\n";
  } else {
    Serial << "# Read command: " << commandBuffer.str() << "\n";
  }
}

void loop() {
  while( Serial.available() > 0 ) {
    TOGoS::Command::TLIBuffer::BufferState bufState = commandBuffer.onChar(Serial.read());
    if( bufState == TOGoS::Command::TLIBuffer::BufferState::READY ) {
      processLine( commandBuffer.str() );
      commandBuffer.reset();
    }
  }
  delay(10);
}
