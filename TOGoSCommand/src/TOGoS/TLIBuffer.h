#ifndef _TOGOS_TLIBUFFER_H
#define _TOGOS_TLIBUFFER_H

#include <TOGoSStringView.h>

namespace TOGoS {
  /** 'Text Line Input Buffer';
   * Buffers characters until a carriage return or newline is read,
   * at which point it switches to READY and the line can be read. */
  class TLIBuffer {
  public:
    enum class BufferState {
      READING,
      READY, // I read a line.  Now take it before feeding me more characters!
      OVERFLOWED
    };

    static const size_t bufferSize = 1024;
  private:
    char buffer[bufferSize];
    size_t pos;
    BufferState state;
  private:
    bool addChar(char c);
  public:
    TLIBuffer();
    /** Clear the buffer and get ready to read a new line */
    void reset();
    /** Call to process the next character; returns the new buffer state.
     * If this returns READY, you should immediately use the value
     * and then call reset() before adding more chars. */
    BufferState onChar(char c);
    StringView str() const;
  };
}

#endif
