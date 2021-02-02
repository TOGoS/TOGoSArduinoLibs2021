#ifndef _TOGOS_COMMAND_COMMANDDISPATCHER_H
#define _TOGOS_COMMAND_COMMANDDISPATCHER_H

#include <functional>
#include <TOGoSStringView.h>
#include "./TokenizedCommand.h"

namespace TOGoS { namespace Command {
  enum class CommandSource {
    CEREAL, // 'SERIAL' is apparently #defined as something in Arduino
    MQTT
  };
  enum class CommandResultCode {
    SHRUG = 0, // Returned when a handler doesn't know what a command is
    HANDLED = 1,
    NOT_ALLOWED = 2, // Returned when a command is recognized but permission denied
  };
  struct CommandResult {
    CommandResultCode code;
    std::string value; // 'return value' if success, error message otherwise
    CommandResult(CommandResultCode code, const TOGoS::StringView &value) : code(code), value(value) {}
    CommandResult(CommandResultCode code) : code(code), value() {}
    static CommandResult ok() { return CommandResult(CommandResultCode::HANDLED); }
    static CommandResult shrug() { return CommandResult(CommandResultCode::SHRUG); }
  };

  using CommandHandler = std::function<CommandResult(const TokenizedCommand &cmd, CommandSource src)>;

  class CommandDispatcher {
    std::vector<CommandHandler> commandHandlers;
  public:
    CommandDispatcher(std::vector<CommandHandler>&& commandHandlers) : commandHandlers(commandHandlers) {}
    CommandResult operator()(const TokenizedCommand &cmd, CommandSource src);
  };
}}

#endif
