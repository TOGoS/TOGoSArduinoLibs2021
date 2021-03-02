#ifndef _TOGOS_COMMAND_COMMANDDISPATCHER_H
#define _TOGOS_COMMAND_COMMANDDISPATCHER_H

#include <functional>
#include <TOGoSStringView.h>
#include "./TokenizedCommand.h"

namespace TOGoS { namespace Command {
  enum class CommandSource {
    CEREAL = 0x01, // 'SERIAL' is apparently #defined as something in Arduino
    MQTT = 0x02
  };
  enum class CommandResultCode {
    SHRUG = 0, // Returned when a handler doesn't know what a command is
    HANDLED = 1,
    FAILED = 2, // Understood and allowed, but failed for some reason
    NOT_ALLOWED = 3, // Returned when a command is recognized but permission denied
    CALLER_ERROR = 4, // Bad arguments, called in wrong state, etc
  };
  struct CommandResult {
    CommandResultCode code;
    std::string value; // 'return value' if success, error message otherwise
    //CommandResult(CommandResultCode code, const TOGoS::StringView &value) : code(code), value(value) {}
    CommandResult(CommandResultCode code, std::string &&value) : code(code), value(std::move(value)) {}
    CommandResult(CommandResultCode code) : code(code), value() {}
    static inline CommandResult ok() { return CommandResult(CommandResultCode::HANDLED); }
    static inline CommandResult ok(std::string &&value) { return CommandResult(CommandResultCode::HANDLED, std::move(value)); }
    static inline CommandResult failed(std::string &&value) { return CommandResult(CommandResultCode::FAILED, std::move(value)); }
    static inline CommandResult shrug() { return CommandResult(CommandResultCode::SHRUG); }
    static inline CommandResult callerError(std::string &&message) { return CommandResult(CommandResultCode::CALLER_ERROR, std::move(message)); }
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
