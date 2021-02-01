#ifndef _TOGOS_COMMAND_TOKENIZE_H
#define _TOGOS_COMMAND_TOKENIZE_H

#include <vector>
#include <TOGoSStringView.h>
#include "./ParseResult.h"

namespace TOGoS { namespace Command {
  ParseResult<std::vector<TOGoS::StringView>> tokenize(const TOGoS::StringView &readFrom, size_t index0);
}}

#endif
