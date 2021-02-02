extern "C" {
	#include <string.h>
}
#include "./MQTTCommandHandler.h"

using StringView = TOGoS::StringView;
using CommandResult = TOGoS::Command::CommandResult;
using MQTTCommandHandler = TOGoS::Command::MQTTCommandHandler;
using TokenizedCommand = TOGoS::Command::TokenizedCommand;

CommandResult MQTTCommandHandler::operator()(const TokenizedCommand &cmd, CommandSource src) {
  if( cmd.path == "mqtt/connected" ) {
    return CommandResult::ok(this->maintainer->pubSubClient->connected() ? "true" : "false");
  } else if( cmd.path == "mqtt/connect" ) {
    StringView username;
    StringView password;
    if( cmd.args.size() == 4 ) {
      username = cmd.args[2];
      password = cmd.args[3];
    } else if( cmd.args.size() == 2 ) {
    } else {
      return CommandResult::callerError( std::string(cmd.path) + " requires 2 or 4 arguments: server, port[, username, password]");
    }
    int port = atoi(std::string(cmd.args[1]).c_str());
    this->maintainer->setServer(cmd.args[0], port, username, password);
    return this->maintainer->update() ? CommandResult::ok("connected") : CommandResult::failed("unable to connect right now, but server/creds set");
  } else if( cmd.path == "mqtt/publush" ) {
    // TODO: Allow '--retain' arguments, etc.
    bool retain = false;
    if( cmd.args.size() != 2 ) {
      return CommandResult::callerError( std::string(cmd.path) + " requires 2 arguments: path, value");
    }
    std::string topic(cmd.args[0]); // No way to indicate to PubSubClient the topic length, so we gotta std::stringify it
    this->maintainer->pubSubClient->publish(topic.c_str(), (const uint8_t *)cmd.args[1].begin(), cmd.args[1].size(), retain);
    return CommandResult::ok();
  } else {
    return CommandResult::shrug();
  }
}
