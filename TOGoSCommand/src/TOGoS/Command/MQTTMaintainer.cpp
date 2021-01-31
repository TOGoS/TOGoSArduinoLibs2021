// Based on the 'Reconnecting MQTT example - non-blocking' example from the PubSub library

#ifdef _TOGOS_COMMAND_MQTTMAINTAINER_DEBUG
#include <TOGoSStreamOperators.h> // For debugging
#endif

#include <functional>
#include <string>
#include <PubSubClient.h>

#include "MQTTMaintainer.h"

using MQTTMaintainer = TOGoS::Command::MQTTMaintainer;

/** non-empty C string; returns nullptr instead of empty strings */
static const char *necs(const std::string& str) {
  return str.empty() ? nullptr : str.c_str();
}

MQTTMaintainer MQTTMaintainer::makeStandard(
  PubSubClient &pubSubClient,
  const TOGoS::StringView &clientId,
  const TOGoS::StringView &topic
) {
  std::string stateTopic = (std::string)topic + "/state";
  
  return MQTTMaintainer(
    pubSubClient,
    clientId,
    stateTopic,
    1, // Deliver will message at least once
    true, // Remember that we're gone so any garbage collectors can do their work
    "disconnected",
    true, // reconnect callback will re-subscribe, etc
    [&pubSubClient, stateTopic]() {
      pubSubClient.publish(stateTopic.c_str(), "online", true);
    }
  );
}

void MQTTMaintainer::setServer(
  const TOGoS::StringView &serverName, uint16_t serverPort,
  const TOGoS::StringView &username, const TOGoS::StringView &password
) {
  // PubSubClient just stores the const char * without copying the contents.
  // Therefore we'd better persist the server name in here!
  this->serverName = serverName;
  this->serverPort = serverPort;
  this->username = username;
  this->password = password;
  this->pubSubClient.setServer(this->serverName.c_str(), serverPort);
}

#ifdef _TOGOS_COMMAND_MQTTMAINTAINER_DEBUG
static std::string quote(const char *s) {
  return std::string("\"") + s + "\"";
}
#endif

bool MQTTMaintainer::reconnect() {
#ifdef _TOGOS_COMMAND_MQTTMAINTAINER_DEBUG
  Serial << "# MQTTMaintainer#reconnect: Attempting to connect to " <<
    this->username << ":" << this->password << "@" <<
    this->serverName << ":" << this->serverPort << "...\n";
  Serial << "# MQTTMaintainer#reconnect: pubSubClient.connect(" <<
    quote(this->clientId.c_str()) << ", " <<
    quote(necs(this->username)) << ", " << necs(this->password) << ", " <<
    quote(necs(this->willTopic)) << ", " <<
    this->willQos << ", " <<
    this->willRetain << ", " <<
    quote(necs(this->willMessage)) << ", " <<
    this->cleanSession << ")...\n";
#endif
  if(
    this->pubSubClient.connect(
      this->clientId.c_str(),
      necs(this->username), necs(this->password),
      necs(this->willTopic),
      this->willQos,
      this->willRetain,
      necs(this->willMessage),
      this->cleanSession
    )
  ) {
#ifdef _TOGOS_COMMAND_MQTTMAINTAINER_DEBUG
    Serial << "# MQTTMaintainer#reconnect: Okay!  Calling reconnectedCallback()\n";
#endif
    this->reconnectedCallback();
    return true;
  } else {
#ifdef _TOGOS_COMMAND_MQTTMAINTAINER_DEBUG
    Serial << "# MQTTMaintainer#reconnect: connection failed; rc:" << this->pubSubClient.state() << "\n";
#endif
    return false;
  }
}
  
bool MQTTMaintainer::update() {
  if( !this->pubSubClient.connected() && !this->serverName.empty() ) {
    long now = millis();
    if( now - this->lastReconnectAttempt > 5000 ) {
      this->lastReconnectAttempt = now;
      // Attempt to reconnect
      if( this->reconnect() ) {
        this->lastReconnectAttempt = 0;
        return true;
      }
    }
    return false;
  } else {
    // Client connected
    this->pubSubClient.loop();
    return true;
  }
}