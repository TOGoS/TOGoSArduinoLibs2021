/*
 * Default wiring:
 * - D1 = I2C shared clock (usually yellow wire)
 * - D2 = I2C 0 data (usually green or white wire)
 * - D3 = I2C 1 data (usually green or white wire)
 * - D4 = I2C 2 data (usually green or white wire)
 * - D7 = I2C 3 data (usually green or white wire)
 * - RST to d0 to enable waking from deep sleep,
 *   but this seems to need to be disconnected during flashing.
 * - 3V3 to peripherals' power + (usually red wire)
 * - D5 to peripheras' power - (this way it will be unpowered while the ESP is asleep) (usually black wire)
 */

//// Begin configuration section

// TODO: Either don't bother with config.h,
// or move this stuff into it

const char *APP_NAME = "EnvironmentalSensor2021";
const char *APP_SHORT_NAME = "ES2021";
const char *APP_VERSION = "0.0.11-dev";
const int BOOTING_BLINKS = 5;
const int GOING_TO_SLEEP_BLINKS = 2;

const char *myHostname = NULL; // "es2021";
bool useStaticIp4 = true;
const byte myIp4[] = {10, 9, 254, 254};
const byte myIp4Gateway[] = {10, 9, 254, 254};
const byte myIp4Subnet[] = {255, 255, 255, 255};
const int myUdpPort = 16378;

#define ES2021_I2C0_SCL D1
#define ES2021_I2C0_SDA D2
#define ES2021_I2C1_SCL D1
#define ES2021_I2C1_SDA D3
#define ES2021_I2C2_SCL D1
#define ES2021_I2C2_SDA D4
#define ES2021_I2C3_SCL D1
#define ES2021_I2C3_SDA D7

// Delay between twiddling between I2C busses, in case that's needed?
const int i2cBusSwitchDelay = 0;
const long shtPollInterval = 5000;

#include "config.h"

//// End configuration section

extern "C" {
  #include <stdlib.h> // atol
}

// ESP8266 board includes 'functional' library, but unforch regular Arduino does not.
// So until I make a replacement (a la TOGoS/StringView), you're stuck with ESP boards.
// Which is fine for me because that's all I ever use anyway.
#include <functional>
#include <vector>

// Arduino libraries
#include <AddrList.h>
#include <ESP8266WiFi.h>
#include <Print.h>
#include <PubSubClient.h>
#include <Wire.h>

// TOGoS libraries
#include <TOGoSBufferPrint.h>
#include <TOGoSCommand.h>
#include <TOGoS/Command/TLIBuffer.h>
#include <TOGoS/Command/ParseError.h>
#include <TOGoS/Command/ParseResult.h>
#include <TOGoS/Command/TokenizedCommand.h>
#include <TOGoS/Command/CommandDispatcher.h>
#include <TOGoS/Command/MQTTMaintainer.h>
#include <TOGoS/Command/MQTTCommandHandler.h>
#include <TOGoSSHT20.h>
#include <TOGoSSSD1306.h>
#include <TOGoS/SSD1306/Printer.h>
#include <TOGoS/SSD1306/font8x8.h>
#include <TOGoSStreamOperators.h>
#include <TOGoSStringView.h>

using CommandHandler = TOGoS::Command::CommandHandler;
using CommandSource = TOGoS::Command::CommandSource;
using CommandResultCode = TOGoS::Command::CommandResultCode;
using CommandResult = TOGoS::Command::CommandResult;
using ParseError = TOGoS::Command::ParseError;
using StringView = TOGoS::StringView;
using TokenizedCommand = TOGoS::Command::TokenizedCommand;
using TCPR = TOGoS::Command::ParseResult<TokenizedCommand>;

//// Additional types

namespace TOGoS { namespace Command {
  class WiFiCommandHandler {
  public:
    CommandResult operator()(const TokenizedCommand &cmd, CommandSource src);
  };
}}

//// Additional functions

char hexDigit(int num) {
  num = num & 0xF;
  if( num < 10 ) return '0' + num;
  if( num < 16 ) return 'A' + num - 10;
  return '?'; // Should be unpossible
}

std::string hexByte(int num) {
  std::string hecks;
  hecks += hexDigit(num >> 4);
  hecks += hexDigit(num);
  return hecks;
}

std::string macAddressToHex(uint8_t *macAddress, const char *octetSeparator) {
  std::string hecks;
  for( int i = 0; i < 6; ++i ) {
    if( i > 0 ) hecks += octetSeparator;
    hecks += hexDigit(macAddress[i] >> 4);
    hecks += hexDigit(macAddress[i]);
  }
  return hecks;
}

void printMacAddressHex(uint8_t *macAddress, const char *octetSeparator, class Print& printer) {
  for( int i = 0; i < 6; ++i ) {
    if( i > 0 ) printer.print(octetSeparator);
    printer.print(hexDigit(macAddress[i] >> 4));
    printer.print(hexDigit(macAddress[i]));
  }
}

const char *formatBool(int boolish) {
  return boolish ? "true" : "false";
}

//// Print overloads

Print& operator<<(Print &printer, const TokenizedCommand &cmd) {
  printer << "path:{" << cmd.path << "}";

  if( cmd.argStr.size() > 0 ) {
    printer << " argStr:{" << cmd.argStr << "}";
  }
  printer << " args:{";
  for( int i=0; i<cmd.args.size(); ++i ) {
    if( i > 0 ) printer << " ";
    printer << "{" << cmd.args[i] << "}";
  }
  printer << "}";
  return printer;
}

Print& operator<<(Print &printer, const ParseError& parseError) {
  return printer << parseError.getErrorMessage() << " at index " << parseError.errorOffset;
}

Print& operator<<(Print &printer, const CommandResultCode& crc) {
  switch( crc ) {
  case CommandResultCode::SHRUG       : return printer << "Unknown command";
  case CommandResultCode::HANDLED     : return printer << "Ok";
  case CommandResultCode::FAILED      : return printer << "Failed";
  case CommandResultCode::NOT_ALLOWED : return printer << "Not allowed";
  case CommandResultCode::CALLER_ERROR: return printer << "Usage error";
  }
  return printer << "Unknown CommandResultCode: " << (int)crc;
}

Print& operator<<(Print& printer, const CommandResult& cresult) {
  printer << cresult.code;
  if( !cresult.value.empty() > 0 ) printer << ": " << cresult.value;
  return printer;
}

//// System-wide global variables

typedef unsigned long Timestamp;

Timestamp currentTime; // Set at beginning of each update

//// WiFi subsystem

// WiFiCommandHandler wifiCommandHandler(); // This confuses the compiler lol.
// TODO: Make a class in TOGoSArduinoLibs that does this
struct WiFiCredz {
  const char *ssid;
  const char *password;
  WiFiCredz(const char *ssid, const char *password) : ssid(ssid), password(password) {}
};

using WiFiCommandHandler = TOGoS::Command::WiFiCommandHandler;

std::vector<WiFiCredz> wifiNetworks;
int wifiNetworkIndex = -1;
Timestamp lastWifiReconnectAttempt = 0;
WiFiCommandHandler wifiCommandHandler;
WiFiClient wifiClient;

void configureWifi(ESP8266WiFiClass &wifi) {
  if( useStaticIp4 ) {
    Serial << "# Configuring with static IPv4 address\n";
    // Unlike SSID/password, stuff config()ured does *not* seem to be retained.
    // So we need to wifi.config(...) each time before wifi.begin(...)ing.
    IPAddress ip4 = myIp4;
    IPAddress ip4Gateway = myIp4Gateway;
    IPAddress ip4Subnet = myIp4Subnet;
    wifi.config(ip4, ip4Gateway, ip4Subnet);
  }
  if( myHostname != NULL ) {
    Serial << "# Configuring hostname\n";
    wifi.hostname(myHostname); // This needs to come after `config`
  }
}

void wifiUpdate() {
  int status = WiFi.status();
  if( status == WL_CONNECTED || status == WL_IDLE_STATUS ) return;
  if( currentTime - lastWifiReconnectAttempt < 5000 ) return;
    
  if( wifiNetworks.size() == 0 ) {
    // Try to auto-connect to whatever's in memory
    Serial << "# No WiFi networks configured.\n";
    Serial << "# Attempting auto-connect to previous network...\n";
    configureWifi(WiFi);
    WiFi.begin();
  } else {
    ++wifiNetworkIndex;
    if( wifiNetworkIndex >= wifiNetworks.size() ) {
      wifiNetworkIndex = 0;
    }
    
    const WiFiCredz &credz = wifiNetworks[wifiNetworkIndex];
    Serial << "# Attempting auto-connect to " << credz.ssid << "...\n";
    configureWifi(WiFi);
    WiFi.begin(credz.ssid, credz.password);
  }
  
  lastWifiReconnectAttempt = currentTime;
}

void printWifiStatus(Print &out) {
  byte macAddressBuffer[6];
  WiFi.macAddress(macAddressBuffer);

  out << F("# Number of wifi networks to which I'll attempt to auto-connect;\n");
  out << F("# (if zero, I may attempt to just begin(); maybe that should be a setting):\n");
  out << "wifi-config/wifi-network-count " << wifiNetworks.size() << "\n";

  if( myHostname != NULL ) {
    out << "net-config/hostname " << myHostname << "\n";
  }
  out << "net-config/use-static-ip4 " << formatBool(useStaticIp4) << "\n";

  out << "wifi/mac-address " << macAddressToHex(macAddressBuffer, ":") << "\n";
  out << "wifi/status-code " << WiFi.status() << "\n";
  out << "wifi/ssid " << WiFi.SSID() << "\n";
  out << "wifi/connected " << formatBool(WiFi.status() == WL_CONNECTED) << "\n";
  out << "wifi/auto-connect " << formatBool(WiFi.getAutoConnect()) << "\n";
  out << "wifi/auto-reconnect " << formatBool(WiFi.getAutoReconnect()) << "\n";

  out << "net/ipv6-enabled " << formatBool(LWIP_IPV6) << "\n";
  for (auto a : addrList) {
    out.printf("net/addr IF='%s' IPv6=%d local=%d hostname='%s' addr= %s",
      a.ifname().c_str(),
      a.isV6(),
      a.isLocal(),
      a.ifhostname(),
      a.toString().c_str());
    
    if (a.isLegacy()) {
      out.printf(" / mask:%s / gw:%s",
        a.netmask().toString().c_str(),
        a.gw().toString().c_str());
    }
    
    out << "\n";
  }
}

CommandResult TOGoS::Command::WiFiCommandHandler::operator()(const TokenizedCommand &cmd, CommandSource src) {
  if( cmd.path == "wifi/connect" ) {
    if( cmd.args.size() == 0 ) {
      configureWifi(WiFi);
      WiFi.begin();
      return CommandResult::ok();
    } else if( cmd.args.size() == 2 ) {
      configureWifi(WiFi);
      WiFi.begin(std::string(cmd.args[0]).c_str(), std::string(cmd.args[1]).c_str());
      return CommandResult::ok();
    } else {
      return CommandResult::callerError(std::string(cmd.path)+" requires either 0 or 2 arguments: ssid, secret");
    }
  } else {
    return CommandResult::shrug();
  }
}

//// MQTT Subsystem

using MQTTMaintainer = TOGoS::Command::MQTTMaintainer;
using MQTTCommandHandler = TOGoS::Command::MQTTCommandHandler;

PubSubClient pubSubClient(wifiClient);
MQTTMaintainer mqttMaintainer = MQTTMaintainer::makeStandard(
  &pubSubClient, getDefaultClientId(), getDefaultClientId());
MQTTCommandHandler mqttCommandHandler(&mqttMaintainer);

std::string getDefaultClientId() {
  byte macAddressBuffer[6];
  WiFi.macAddress(macAddressBuffer);
  return macAddressToHex(macAddressBuffer, ":");
}

void printMqttStatus(Print &out) {
  out << "mqtt/server/host " << mqttMaintainer.serverName << "\n";
  out << "mqtt/server/port " << mqttMaintainer.serverPort << "\n";
  out << "mqtt/connected " << (mqttMaintainer.isConnected() ? "true" : "false") << "\n";
}

// More MQTT stuff is scattered through the rest of the program.


//// Peripherals

struct ES2021Reading {
  Timestamp time;
  TOGoS::SHT20::EverythingReading data = TOGoS::SHT20::EverythingReading();
};

// TODO: Replace these ifdefs scattered all over with
// iterating over a constant (in ROM?) table of the sensors
// and related parameters.

#ifdef ES2021_I2C0_SDA
// SSD1306 implicitly on the first I2C bus.
//
// Compiler: "'Wire' does not name a type"
// 
// Me: WTF, this is horsecrap
// 
// Me later: Oh.  TwoWire is the class.  Wire is the instance.
// This is what happens when you break naming conventions!
// People get confused!
TOGoS::SSD1306::Driver ssd1306(Wire);

ES2021Reading temhum0Cache;
#endif
#ifdef ES2021_I2C1_SDA
ES2021Reading temhum1Cache;
#endif
#ifdef ES2021_I2C2_SDA
ES2021Reading temhum2Cache;
#endif
#ifdef ES2021_I2C3_SDA
ES2021Reading temhum3Cache;
#endif
Timestamp latestMqttPublishTime;

const uint8_t PUB_SERIAL = 0x1;
const uint8_t PUB_MQTT   = 0x2;
// HELO packets handled separately

//// HELO

#include <WiFiUdp.h>

class HeloModule {
  long lastBroadcast = -1;

  WiFiUDP udp;
public:
  void update(long currentTime);
};


void printTemhumData(const char *devName, const ES2021Reading &cache, Print &out);

void printTemhumData(const char *devName, const ES2021Reading &cache, Print &out) {
  const TOGoS::SHT20::EverythingReading &reading = cache.data;
  out << devName << "/read" << " " << (cache.time / 1000.0) << "\n";
  //out << devName << "/reading/age" << " "<< ((currentTime - cache.time) / 1000.0) << "\n";
  out << devName << "/connected" << " " << formatBool(reading.isValid()) << "\n";
  if( reading.isValid() ) {
    out << devName << "/temperature/c" << " " << reading.getTemperatureC() << "\n";
    out << devName << "/temperature/f" << " " << reading.getTemperatureF() << "\n";
    out << devName << "/humidity/percent" << " " << reading.getRhPercent() << "\n";
  }
}

void HeloModule::update(long currentTime) {
  if( currentTime - lastBroadcast > 10000 ) {
    byte macAddressBuffer[6];
    WiFi.macAddress(macAddressBuffer);
    
    char buf[1024];
    TOGoS::BufferPrint bufPrn(buf, sizeof(buf));
    
    //bufPrn << "#HELO //" << macAddressToHex(macAddressBuffer, "-") << "/\n";
    bufPrn << "#HELO\n";
    bufPrn << "\n";
    bufPrn << "app-name " << APP_NAME << "\n";
    bufPrn << "app-version " << APP_VERSION << "\n";
    bufPrn << "mac " << macAddressToHex(macAddressBuffer, ":") << "\n";
    bufPrn << "clock " << currentTime << "\n";
    
#ifdef ES2021_I2C0_SDA
    printTemhumData("temhum0", temhum0Cache, bufPrn);
#endif
#ifdef ES2021_I2C1_SDA
    printTemhumData("temhum1", temhum1Cache, bufPrn);
#endif
#ifdef ES2021_I2C2_SDA
    printTemhumData("temhum2", temhum2Cache, bufPrn);
#endif
#ifdef ES2021_I2C3_SDA
    printTemhumData("temhum3", temhum3Cache, bufPrn);
#endif
    
    Serial << "# Broadcasting a HELO packet\n";
    
    udp.beginPacket("ff02::1", myUdpPort);
    udp.write(buf, bufPrn.size());
    udp.endPacket();
    
    lastBroadcast = currentTime;
  }
}


HeloModule heloModule;

//// Stateful functions

int currentSda = -1;
int currentScl = -1;

void setWireBus(int sda, int scl) {
  // Not sure if there's any reason to skip it,
  // but if we do want to...
  if( currentSda != sda || currentScl != scl ) {
    Wire.begin(sda, scl);
    currentSda = sda;
    currentScl = scl;
    delay(i2cBusSwitchDelay);
  }
}

const TOGoS::SHT20::EverythingReading &updateSht20Reading(ES2021Reading &cache, long currentTime, int sda, int scl) {
  TOGoS::SHT20::Driver sht20(Wire);
  setWireBus(sda, scl);
  delay(i2cBusSwitchDelay);
  cache.data = sht20.readEverything();
  cache.time = currentTime;
  return cache.data;
}

void blinkBuiltin(int count) {
  for( ; count > 0 ; --count ) {
    delay(74);
    digitalWrite(LED_BUILTIN, LOW);
    delay(150);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(75);
  }
}

/// Constant printers

#define PRINTCONST(CONSTNAME) out << "constants/" << #CONSTNAME << " " << CONSTNAME << "\n"

void printPins(Print &out) {
  out << "# Pins constants:\n";
  PRINTCONST(D0);
  PRINTCONST(D1);
  PRINTCONST(D2);
  PRINTCONST(D3);
  PRINTCONST(D4);
  PRINTCONST(D5);
  PRINTCONST(D6);
  PRINTCONST(D7);
  PRINTCONST(D8);
  PRINTCONST(LED_BUILTIN);
}

void printConstants(Print &out) {
  printPins(out);
  out << "# WiFi status constants:\n";
  PRINTCONST(WL_NO_SHIELD);
  PRINTCONST(WL_IDLE_STATUS);
  PRINTCONST(WL_NO_SSID_AVAIL);
  PRINTCONST(WL_SCAN_COMPLETED);
  PRINTCONST(WL_CONNECTED);
  PRINTCONST(WL_CONNECT_FAILED);
  PRINTCONST(WL_CONNECTION_LOST);
  PRINTCONST(WL_DISCONNECTED);
}

#define PRINTCONST

struct Henlo {} HENLO;

Print &operator<<(Print &p, const struct Henlo &hi) {
  return p << "# Hello from " << APP_NAME << " v" << APP_VERSION << "!";
}

CommandResult readSht20Cmd(ES2021Reading &cache, int sda, int scl);

CommandResult readSht20Cmd(ES2021Reading &cache, int sda, int scl) {
  char buf[64];
  TOGoS::BufferPrint bufPrn(buf, sizeof(buf));
  const TOGoS::SHT20::EverythingReading &reading = updateSht20Reading(cache, currentTime, sda, scl);
  if( reading.isValid() ) {
    bufPrn << "temp:" << reading.getTemperatureC() << "C" << " "
           << "humid:" << reading.getRhPercent() << "%";
    return CommandResult::ok(std::string(bufPrn.str()));
  } else {
    return CommandResult::failed("not connected");
  }
}

CommandResult processEs2021Command(const TokenizedCommand &tcmd, CommandSource source) {
  if( tcmd.path == "echo" ) {
    for( int i=0; i<tcmd.args.size(); ++i ) {
      if( i > 0 ) Serial << " ";
      Serial << tcmd.args[i];
    }
    Serial << "\n";
    return CommandResult::ok();
  } else if( tcmd.path == "echo/lines" ) {
    for( auto arg : tcmd.args ) {
      Serial << arg << "\n";
    }
    return CommandResult::ok();
  } else if( tcmd.path == "status" ) {
    printWifiStatus(Serial);
    printMqttStatus(Serial);
    return CommandResult::ok();
  } else if( tcmd.path == "constants" ) {
    printConstants(Serial);
    return CommandResult::ok();
  } else if( tcmd.path == "pins" ) {
    printPins(Serial);
    return CommandResult::ok();
  } else if( tcmd.path == "hi" || tcmd.path == "hello" ) {
    Serial << HENLO << "\n";
    return CommandResult::ok();
  } else if( tcmd.path == "digital-write" ) {
    if( tcmd.args.size() != 2 ) {
      return CommandResult::callerError("Usage: digitalwrite <pin : uint> <value : 0|1>");
    }
    digitalWrite(atoi(std::string(tcmd.args[0]).c_str()), atoi(std::string(tcmd.args[1]).c_str()));
    return CommandResult::ok();
#ifdef ES2021_I2C0_SDA
  } else if( tcmd.path == "temhum0/read" ) {
    return readSht20Cmd(temhum0Cache, ES2021_I2C0_SDA, ES2021_I2C0_SCL);
#endif
#ifdef ES2021_I2C1_SDA
  } else if( tcmd.path == "temhum1/read" ) {
    return readSht20Cmd(temhum1Cache, ES2021_I2C1_SDA, ES2021_I2C1_SCL);
#endif
#ifdef ES2021_I2C2_SDA
  } else if( tcmd.path == "temhum2/read" ) {
    return readSht20Cmd(temhum2Cache, ES2021_I2C2_SDA, ES2021_I2C2_SCL);
#endif
#ifdef ES2021_I2C3_SDA
  } else if( tcmd.path == "temhum3/read" ) {
    return readSht20Cmd(temhum3Cache, ES2021_I2C3_SDA, ES2021_I2C3_SCL);
#endif
  } else if( tcmd.path == "help" ) {
    Serial << HENLO << "\n";
    Serial << "# \n";
    Serial << "# Commands:\n";
    Serial << "#   echo $arg1 .. $argN\n";
    Serial << "#   echo/lines $arg1 .. $argN\n";
    Serial << "#   status ; print status of WiFi, MQTT connection, etc\n";
    Serial << "#   pins ; print pin information\n";
    Serial << "#   wifi/connect [$ssid $password] ; connect to a WiFi network\n";
    Serial << "#   temhum0/read ; Read values from temperature/humidity device 0\n";
    Serial << "#   temhum1/read ; Read values from temperature/humidity device 1\n";
    Serial << "#   mqtt/connect $server $port $username $password ; connect to an MQTT server\n";
    Serial << "#   mqtt/disconnect ; disconnect/stop trying to connect to any MQTT server\n";
    Serial << "#   mqtt/publish $topic $value ; publish to MQTT\n";
    Serial << "#   deepsleep $seconds ; go into deep sleep\n";
    Serial << "# \n";
    Serial << "# Use curly braces to quote {multi-word arguments}\n";
    Serial << "# \n";
    Serial << "# WiFi and MQTT connections will automatically reconnect\n";
    Serial << "# unless server name is empty string (represented by '{}')\n";
    return CommandResult::ok();
  } else if( tcmd.path == "deepsleep" ) {
    if( tcmd.args.size() != 1 ) {
      return CommandResult::callerError("Usage: deepsleep <time : seconds>");
    } else {
      long sleeptime = atol(std::string(tcmd.args[0]).c_str());
      Serial << "# Going into deep sleep for " << sleeptime << "s\n";
      blinkBuiltin(GOING_TO_SLEEP_BLINKS); // 3 blinks = going to sleep
      ESP.deepSleep(sleeptime * 1000000);
      return CommandResult::ok(); // Probably never get here lol
    }
  } else if( tcmd.path == "bye" ) {
    Serial << "# See ya later!\n";
    return CommandResult::ok();
  } else {
    return CommandResult::shrug();
  }
}

TOGoS::Command::CommandDispatcher serialCommandDispatcher({
  processEs2021Command,
  wifiCommandHandler,
  mqttCommandHandler,
});

void processSerialLine(const StringView& line) {
  TCPR pr = TokenizedCommand::parse(line);
  if( pr.isError() ) {
    Serial << "# Error parsing command: " << pr.error << "\n";
    return;
  }

  const TokenizedCommand &tcmd = pr.value;

  if( tcmd.path.size() == 0 ) return;
  
  CommandResult cresult = serialCommandDispatcher(tcmd, CommandSource::CEREAL);
  Serial << "# " << tcmd.path << ": " << cresult << "\n";
}

using TLIBuffer = TOGoS::Command::TLIBuffer;

TLIBuffer commandBuffer;

void drawBackground() {
  TOGoS::SSD1306::Printer printer(ssd1306, TOGoS::SSD1306::font8x8);
  
  ssd1306.clear();
  ssd1306.gotoRowCol(0,0);
  printer << APP_SHORT_NAME << " " << APP_VERSION;
  
  drawConnectionIndicators();
}

// TODO: Have a method on printer to print arbitrary glyphs
uint8_t wifiIcon[]   = { 0x02, 0x09, 0x05, 0x55, 0x05, 0x09, 0x02, 0x00 };
uint8_t noWifiIcon[] = { 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00 };

void printGlyph(TOGoS::SSD1306::Driver &driver, const uint8_t *data, int size) {
  for( int i=0; i<size; ++i ) {
    driver.sendData(data[i]);
  }
}

void drawConnectionIndicators() {
  TOGoS::SSD1306::Printer printer(ssd1306, TOGoS::SSD1306::font8x8);
  
  ssd1306.gotoRowCol(0,112);
  printGlyph(ssd1306, WiFi.status() == WL_CONNECTED ? wifiIcon : noWifiIcon, 8);
  
  ssd1306.gotoRowCol(0,120);
  printer << (mqttMaintainer.isConnected() ? "M" : ".");
}

// TODO: I guess I need a different staleness bar per sensor?
// Unless there's only one.  :-P
void redrawStalenessBar() {
  int screenWidth = ssd1306.getColumnCount();
  ssd1306.gotoRowCol(1,0);
  long timeSincePreviousReading = millis() - temhum0Cache.time;
  int stalenessBarWidth = screenWidth * std::min((unsigned int)4000,(unsigned int)timeSincePreviousReading) / 4000;
  for( int i=0; i<stalenessBarWidth; ++i ) {
    ssd1306.sendData(0x42);
  }
  for( int i=stalenessBarWidth; i<screenWidth; ++i ) {
    ssd1306.sendData(0x7E);
  }
}

void publishAttrToMqtt(const char *attrName, const char *value) {
  char topicBuffer[64];
  TOGoS::BufferPrint topicPrn(topicBuffer, sizeof(topicBuffer));
  topicPrn << getDefaultClientId() << "/" << attrName;  
  Serial << "mqtt/publish " << topicPrn.c_str() << " " << value << "\n";
  pubSubClient.publish(topicPrn.c_str(), value);
}
template <typename T>
void publishAttrToMqtt(const char *attrName, T value) {
  char textBuf[64];
  TOGoS::BufferPrint bufPrn(textBuf, sizeof(textBuf));
  bufPrn << value;
  publishAttrToMqtt(attrName, bufPrn.c_str());
}

template <typename T>
void publishAttr(const char *topic, T value, uint8_t dest) {
  if( dest & PUB_SERIAL ) Serial << topic << " " << value << "\n";
  if( dest & PUB_MQTT   ) publishAttrToMqtt(topic, value);
}

void reportReading(const std::string &devName, const ES2021Reading &cache, uint8_t dest);

void reportReading(const std::string &devName, const ES2021Reading &cache, uint8_t dest) {
  const TOGoS::SHT20::EverythingReading &reading = cache.data;
  publishAttr((devName + "/reading/age").c_str(), ((currentTime - cache.time) / 1000.0), dest);
  publishAttr((devName + "/connected").c_str(), formatBool(reading.isValid()), dest);
  if( reading.isValid() ) {
    publishAttr((devName + "/temperature/c").c_str(), reading.getTemperatureC(), dest);
    publishAttr((devName + "/temperature/f").c_str(), reading.getTemperatureF(), dest);
    publishAttr((devName + "/humidity/percent").c_str(), reading.getRhPercent(), dest);
  }
}

void redrawReading(const ES2021Reading &cache);
void redrawReading(const ES2021Reading &cache) {
  TOGoS::SSD1306::Printer printer(ssd1306, TOGoS::SSD1306::font8x8);

  // TODO: Put something on screen to show WiFi/MQTT connectivity

  ssd1306.gotoRowCol(2,0);
  printer << "Temperature:";
  printer.clearToEndOfRow();
  
  ssd1306.gotoRowCol(3,12);
  if( cache.data.temperature.isValid() ) {
    printer.print(cache.data.getTemperatureF(),1);
    printer << "F / ";
    printer.print(cache.data.getTemperatureC(),1);
    printer << "C";
  } else {
    printer << "----";
  }
  printer.clearToEndOfRow();
  
  ssd1306.gotoRowCol(4,0);
  printer << "Humidity:";
  printer.clearToEndOfRow();
  
  ssd1306.gotoRowCol(5,12);
  if( cache.data.humidity.isValid() ) {
    printer.print(cache.data.getRhPercent(),1);
    printer << "%";
  } else {
    printer << "----";
  }
  printer.clearToEndOfRow();
  
  ssd1306.gotoRowCol(6,0);
  printer << "VPD: ";
  if( cache.data.isValid() ) {
    printer.print(cache.data.getVpdKpa(),1);
    printer << " kPa";
  }
  printer.clearToEndOfRow();
  
  ssd1306.gotoRowCol(7,0);
  if( cache.data.isValid() ) {
    printer << "DP : ";
    printer.print(cache.data.getDewPoint().getF(),0);
    printer << " F";
  }
  printer.clearToEndOfRow();
}

Timestamp lastReadingRedraw = 0;
Timestamp lastConnectionRedraw = 0;

void maybeUpdateSht20Reading(ES2021Reading &cache, long currentTime, int sda, int scl);

void maybeUpdateSht20Reading(ES2021Reading &cache, long currentTime, int sda, int scl) {
  if( currentTime - cache.time > shtPollInterval ) {
    updateSht20Reading(cache, currentTime, sda, scl);
  }
}

// Use these in your ES2021_POST_SETUP_CPP, if you like:

inline void addWifiNetwork(const char *ssid, const char *password) {
  wifiNetworks.emplace_back(ssid, password);
}
inline void setMqttServer(const char *hostName, uint16_t port, const char *username, const char *password) {
  mqttMaintainer.setServer(hostName, port, username, password);
}

void setup() {
  delay(500); // Standard 'give me time to reprogram it' delay in case the program messes up some I/O pins
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D5, OUTPUT);
  pinMode(D7, OUTPUT);
  Serial << "\n\n"; // Get past any junk in the terminal
  Serial << "# " << APP_NAME << " setup()...\n";
  blinkBuiltin(BOOTING_BLINKS); // 5 blinks = booting, same as ArduinoPowerSwitch
  Serial << "# Initializing I2C...\n";
  
  setWireBus(ES2021_I2C0_SDA, ES2021_I2C0_SCL);
  
  Serial << "# Initializing SSD1306 (0x" << hexByte(SSD1306_ADDRESS) << ")...\n";
  ssd1306.begin(SSD1306_ADDRESS);
  Serial << "# And writing to it...\n";
  ssd1306.displayOn();
  ssd1306.setBrightness(255);
  drawBackground();
  
  Serial << "# We managed to not crash during boot!\n";
  Serial << "# Type 'help' for help.\n";

#ifdef ES2021_I2C0_SDA
  temhum0Cache.time    = millis() - 100000;
#endif
#ifdef ES2021_I2C1_SDA
  temhum1Cache.time    = millis() - 100000;
#endif
#ifdef ES2021_I2C2_SDA
  temhum2Cache.time    = millis() - 100000;
#endif
#ifdef ES2021_I2C3_SDA
  temhum3Cache.time    = millis() - 100000;
#endif

  latestMqttPublishTime = millis() - 100000;

#ifdef ES2021_POST_SETUP_CPP
  ES2021_POST_SETUP_CPP
#endif
}

void loop() {
  currentTime = millis();
  
  while( Serial.available() > 0 ) {
    TLIBuffer::BufferState bufState = commandBuffer.onChar(Serial.read());
    if( bufState == TLIBuffer::BufferState::READY ) {
      processSerialLine( commandBuffer.str() );
      commandBuffer.reset();
    }
  }
  
  delay(10);
  mqttMaintainer.update();

  Timestamp lastReadingUpdate = 0;
  bool publishToMqtt = currentTime - latestMqttPublishTime > 15000;
  uint8_t pubDest = PUB_SERIAL;
  
  // Hmm: this logic is a little weird now that there are multiple sensors.
  // Need to decouple reading/publishing more.
  if( publishToMqtt && mqttMaintainer.isConnected() ) {
    pubDest |= PUB_MQTT;
    Serial << "# Publishing to " << getDefaultClientId() << "/...\n";
    latestMqttPublishTime = currentTime; // Assuming we really are connected lmao
  }
  
#ifdef ES2021_I2C0_SDA
  maybeUpdateSht20Reading(temhum0Cache, currentTime, ES2021_I2C0_SDA, ES2021_I2C0_SCL);
  lastReadingUpdate = max(temhum0Cache.time, lastReadingUpdate);
  reportReading("temhum0", temhum0Cache, pubDest);
#endif
#ifdef ES2021_I2C1_SDA
  maybeUpdateSht20Reading(temhum1Cache, currentTime, ES2021_I2C1_SDA, ES2021_I2C1_SCL);
  lastReadingUpdate = max(temhum1Cache.time, lastReadingUpdate);
  reportReading("temhum1", temhum0Cache, pubDest);
#endif
#ifdef ES2021_I2C2_SDA
  maybeUpdateSht20Reading(temhum2Cache, currentTime, ES2021_I2C2_SDA, ES2021_I2C2_SCL);
  lastReadingUpdate = max(temhum2Cache.time, lastReadingUpdate);
  reportReading("temhum2", temhum0Cache, pubDest);
#endif
#ifdef ES2021_I2C3_SDA
  maybeUpdateSht20Reading(temhum3Cache, currentTime, ES2021_I2C3_SDA, ES2021_I2C3_SCL);
  lastReadingUpdate = max(temhum3Cache.time, lastReadingUpdate);
  reportReading("temhum3", temhum0Cache, pubDest);
#endif
  
  setWireBus(ES2021_I2C0_SDA, ES2021_I2C0_SCL);
  
  if( currentTime - lastConnectionRedraw > 1000 ) {
    drawConnectionIndicators();
    lastConnectionRedraw = currentTime;
  }
  
  if( lastReadingUpdate > lastReadingRedraw ) {
    // TODO: Draw them all?
#ifdef ES2021_I2C0_SDA
    redrawReading(temhum0Cache);
#endif
    lastReadingRedraw = currentTime;
  }
  
  Serial << "\n";
  
  redrawStalenessBar();
  
  wifiUpdate();
  heloModule.update(currentTime);
}
