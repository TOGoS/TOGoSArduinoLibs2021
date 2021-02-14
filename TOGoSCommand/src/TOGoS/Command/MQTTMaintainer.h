// Based on the 'Reconnecting MQTT example - non-blocking' example from the PubSub library
#ifndef _TOGOS_COMMAND_MQTTMAINTAINER_H
#define _TOGOS_COMMAND_MQTTMAINTAINER_H

#define _TOGOS_COMMAND_MQTTMAINTAINER_DEBUG true

#include <functional>
#include <string>
#include <PubSubClient.h>
#include <TOGoSStringView.h>

// MQTT QoS

namespace TOGoS { namespace Command {

/**
 * A wrapper around PubSubClient to make things more convenient
 * for my programs.
 *
 * The provided PubSubClient should already be associated with a 'client'
 * (e.g. a WiFiClient or an ethernet one), otherwise connect() will crash!
 * Whether its client is nullptr is private, see.
 * 
 * Maybe in the future I will fully wrap the PubSubClient so that can't be messed up.
 */    
class MQTTMaintainer {
  using Callback = std::function<void()>;

public:
  PubSubClient *pubSubClient;
  std::string serverName;
  uint16_t serverPort;
  std::string clientId;
  std::string username;
protected:
  std::string password;
  std::string willTopic;
  uint8_t willQos;
  bool willRetain;
  std::string willMessage;
  bool cleanSession;
  Callback reconnectedCallback;
  
  long lastReconnectAttempt = 0;
  
public:
  /** Initializing server info is a /separate step/ */
  MQTTMaintainer(PubSubClient *pubSubClient,
		 const TOGoS::StringView &clientId,
                 const TOGoS::StringView &willTopic,
                 uint8_t willQos,
                 bool willRetain,
                 const TOGoS::StringView &willMessage,
                 bool cleanSession,
		 Callback reconnectedCallback)
    : pubSubClient(pubSubClient),
      serverName(),
      serverPort(),
      clientId(clientId),
      username(),
      password(),
      willTopic(willTopic),
      willQos(willQos),
      willRetain(willRetain),
      willMessage(willMessage),
      cleanSession(cleanSession),
      reconnectedCallback(reconnectedCallback) { }

protected:
  bool reconnect();
public:

  /** Create an MQTTMaintainer with the 'standard' settings; */
  static MQTTMaintainer makeStandard(PubSubClient *pubSubClient,
                                     const TOGoS::StringView &clientId,
                                     const TOGoS::StringView &topic);

  void setServer(const TOGoS::StringView &serverName, uint16_t serverPort,
                 const TOGoS::StringView &username, const TOGoS::StringView &password);

  /** If server has been set and not connected, try to connect
   * (waiting between attempts) */
  bool update();
  bool isConnected();
};

}}

#endif
