#ifndef TOGOS_BUFFERPRINT_H
#define TOGOS_BUFFERPRINT_H

#include <Print.h>

#include <TOGoSStringView.h>

namespace TOGoS {
  class BufferPrint : public Print {
    char *buffer;
    const size_t bufferSize;
    size_t messageEnd = 0;
  public:
    BufferPrint(char *buffer, size_t bufferSize) : buffer(buffer), bufferSize(bufferSize) {}
    
    void clear() {
      messageEnd = 0;
    }
    
    virtual size_t write(uint8_t c) override {
      if( messageEnd < bufferSize - 1 ) {
        buffer[messageEnd++] = c;
        return 1;
      } else return 0;
    }
    
    size_t write(const uint8_t *stuff, size_t len) override {
      size_t maxLen = bufferSize - messageEnd;
      if( len > maxLen ) len = maxLen;
      for( size_t i=len; i-- > 0; ) buffer[messageEnd++] = *stuff++;
      return len;
    }
    
    virtual int availableForWrite() const {
      return this->bufferSize - this->messageEnd;
    }
    
    const char *getBuffer() const {
      return buffer;
    }
    size_t size() const {
      return messageEnd;
    }
    StringView str() const {
      return StringView(buffer, messageEnd);
    }
  };
}

#endif
