#include "./MQTTMaintainer.h"
#include "./TokenizedCommand.h"
#include "./CommandDispatcher.h"

namespace TOGoS { namespace Command {
  // Handles commands *about* the MQTT connection
  class MQTTCommandHandler {
    MQTTMaintainer *maintainer;
  public:
    MQTTCommandHandler(MQTTMaintainer *maintainer) : maintainer(maintainer) { }
    CommandResult operator()(const TokenizedCommand &cmd, CommandSource src);
  };
}}
