#include "TLIBuffer.h"

using TLIBuffer = TOGoS::TLIBuffer;
using StringView = TOGoS::StringView;

TLIBuffer::TLIBuffer() {
  this->reset();
}
void TLIBuffer::reset() {
  this->buffer[bufferSize-1] = 0;
  this->pos = 0;
  this->state = BufferState::READING;
}
bool TLIBuffer::addChar(char c) {
  if( this->state == BufferState::READY ) return false;
  if( pos >= bufferSize - 1 ) return false;
  buffer[pos++] = c;
  return true;
}
TLIBuffer::BufferState TLIBuffer::onChar(char c) {
  if( this->state == BufferState::READY ) {
    // Clear it out!
    pos = 0;
  }
  if( c == '\n' || c == '\r' ) {
    buffer[pos] = 0;
    return this->state = BufferState::READY;
  }
  if( addChar(c) ) return this->state = BufferState::READING;
  return this->state = BufferState::OVERFLOWED;
}
StringView TLIBuffer::str() const {
  return StringView(buffer, pos);
}
