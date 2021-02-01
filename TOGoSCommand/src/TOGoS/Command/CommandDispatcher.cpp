#include "./CommandDispatcher.h"

using TokenizedCommand = TOGoS::Command::TokenizedCommand;
using CommandSource = TOGoS::Command::CommandSource;
using CommandResultCode = TOGoS::Command::CommandResultCode;
using CommandResult = TOGoS::Command::CommandResult;
CommandResult TOGoS::Command::CommandDispatcher::operator()(const TokenizedCommand &tcmd, CommandSource source) {
  for( auto &handler : this->commandHandlers ) {
    CommandResult handlerResult = handler(tcmd, source);
    if( handlerResult.code != CommandResultCode::SHRUG ) return handlerResult; 
  }
  return CommandResult(CommandResultCode::SHRUG, std::string("Unrecognized command: ") + std::string(tcmd.path));
}
