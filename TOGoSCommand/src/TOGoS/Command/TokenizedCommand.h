#ifndef _TOGOS_COMMAND_TOKENIZEDCOMMAND_H
#define _TOGOS_COMMAND_TOKENIZEDCOMMAND_H

#include <vector>
#include <TOGoSStringView.h>
#include "./ParseError.h"
#include "./ParseResult.h"

namespace TOGoS { namespace Command {
  class TokenizedCommand {
  public:
    const TOGoS::StringView path;
    const TOGoS::StringView argStr;
    const std::vector<TOGoS::StringView> args;

    TokenizedCommand(const TOGoS::StringView& path, const TOGoS::StringView& argStr, const std::vector<TOGoS::StringView> args);
    TokenizedCommand();

    bool isEmpty() const {
      return this->path.size() == 0;
    }

    static inline TokenizedCommand empty() {
      return TokenizedCommand();
    }
    
    static ParseResult<TokenizedCommand> parse(const TOGoS::StringView &input);
  };
}}

#endif
