#ifndef _TOGOS_COMMAND_TOKENIZEDCOMMAND_H
#define _TOGOS_COMMAND_TOKENIZEDCOMMAND_H

#include <vector>
#include <TOGoSStringView.h>
#include "./ParseError.h"
#include "./ParseResult.h"

namespace TOGoS { namespace Command {
  using StringView = TOGoS::StringView;
  using TokenList = std::vector<StringView>;

  class TokenizedCommand {
  public:
    const StringView path;
    const StringView argStr;
    const TokenList args;

    TokenizedCommand(const StringView& path, const StringView& argStr, const std::vector<StringView> args);
    TokenizedCommand();

    bool isEmpty() const {
      return this->path.size() == 0;
    }

    static inline TokenizedCommand empty() {
      return TokenizedCommand();
    }
    
    static ParseResult<TokenizedCommand> parse(const StringView &input);
  };
  
  ParseResult<TokenList> tokenize(const StringView& readFrom, size_t index0);
}}

#endif
