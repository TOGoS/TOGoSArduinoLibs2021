const char *APP_NAME = "EnvironmentalSensor2021";
const char *APP_SHORT_NAME = "ES2021";
const char *APP_VERSION = "0.0.9-dev";
const int BOOTING_BLINKS = 5;
const int GOING_TO_SLEEP_BLINKS = 2;

/*
 * Wiring:
 * - D1 = I2C clock
 * - D2 = I2C data
 * - RST to d0 to enable waking from deep sleep,
 *   but this seems to need to be disconnected during flashing.
 * - 3V3 to peripherals' power +
 * - D5 to peripheras' power - (this way it will be unpowered while the ESP is asleep)
 */

#include "config.h"

extern "C" {
  #include <stdlib.h> // atol
}

// ESP8266 board includes 'functional' library, but unforch regular Arduino does not.
// So until I make a replacement (a la TOGoS/StringView), you're stuck with ESP boards.
// Which is fine for me because that's all I ever use anyway.
#include <functional>
#include <vector>

// Arduino libraries
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

//// Pure-ish functions

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

//// Additional types

namespace TOGoS { namespace Command {
  class WiFiCommandHandler {
  public:
    CommandResult operator()(const TokenizedCommand &cmd, CommandSource src);
  };
}}

//// Global variables

using MQTTMaintainer = TOGoS::Command::MQTTMaintainer;
using MQTTCommandHandler = TOGoS::Command::MQTTCommandHandler;
using WiFiCommandHandler = TOGoS::Command::WiFiCommandHandler;

// WiFiCommandHandler wifiCommandHandler(); // This confuses the compiler lol.
WiFiCommandHandler wifiCommandHandler;
WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);
MQTTMaintainer mqttMaintainer = MQTTMaintainer::makeStandard(
  &pubSubClient, getDefaultClientId(), getDefaultClientId());
MQTTCommandHandler mqttCommandHandler(&mqttMaintainer);

// Compiler: "'Wire' does not name a type"
// 
// Me: WTF, this is horsecrap
// 
// Me later: Oh.  TwoWire is the class.  Wire is the instance.
// This is what happens when you break naming conventions!
// People get confused!
TwoWire i2c;
TOGoS::SHT20::Driver sht20(i2c);
TOGoS::SSD1306::Driver ssd1306(i2c);

struct ES2021Reading {
  unsigned long time;
  TOGoS::SHT20::EverythingReading data = TOGoS::SHT20::EverythingReading();
} latestReading;
unsigned long currentTime; // Set at beginning of each update
unsigned long latestMqttPublishTime;

const uint8_t PUB_SERIAL = 0x1;
const uint8_t PUB_MQTT   = 0x2;

//// Stateful functions

const TOGoS::SHT20::EverythingReading &updateReading() {
  latestReading.data = sht20.readEverything();
  latestReading.time = currentTime;
  return latestReading.data;
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

CommandResult TOGoS::Command::WiFiCommandHandler::operator()(const TokenizedCommand &cmd, CommandSource src) {
  if( cmd.path == "wifi/connect" ) {
    if( cmd.args.size() != 2 ) {
      return CommandResult::callerError(std::string(cmd.path)+" requires 2 arguments: ssid, secret");
    }
    WiFi.begin(std::string(cmd.args[0]).c_str(), std::string(cmd.args[1]).c_str());
    return CommandResult::ok();
  } else {
    return CommandResult::shrug();
  }
}

std::string getDefaultClientId() {
  byte macAddressBuffer[6];
  WiFi.macAddress(macAddressBuffer);
  return macAddressToHex(macAddressBuffer, ":");
}

void printWifiStatus() {
  byte macAddressBuffer[6];
  WiFi.macAddress(macAddressBuffer);

  Serial << "wifi/mac-address " << macAddressToHex(macAddressBuffer, ":") << "\n";
  Serial << "wifi/status-code " << WiFi.status() << "\n";
  Serial << "wifi/ssid " << WiFi.SSID() << "\n";
  Serial << "wifi/connected " << (WiFi.status() == WL_CONNECTED ? "true" : "false") << "\n";
}

void printMqttStatus() {
  Serial << "mqtt/server/host " << mqttMaintainer.serverName << "\n";
  Serial << "mqtt/server/port " << mqttMaintainer.serverPort << "\n";
  Serial << "mqtt/connected " << (mqttMaintainer.isConnected() ? "true" : "false") << "\n";
}

void printPins() {
  Serial << "# Pins constants:\n";
  Serial << "#  D0 = " << D0 << "\n";
  Serial << "#  D1 = " << D1 << "\n";
  Serial << "#  D2 = " << D2 << "\n";
  Serial << "#  D3 = " << D3 << "\n";
  Serial << "#  D4 = " << D4 << "\n";
  Serial << "#  D5 = " << D5 << "\n";
  Serial << "#  D6 = " << D6 << "\n";
  Serial << "#  D7 = " << D7 << "\n";
  Serial << "#  D8 = " << D8 << "\n";
  Serial << "#  LED_BUILTIN = " << LED_BUILTIN << "\n";
}

struct Henlo {} HENLO;

Print &operator<<(Print &p, const struct Henlo &hi) {
  return p << "# Hello from " << APP_NAME << " v" << APP_VERSION << "!";
}

CommandResult processEchoCommand(const TokenizedCommand& tcmd, CommandSource source) {
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
    printWifiStatus();
    printMqttStatus();
    return CommandResult::ok();
  } else if( tcmd.path == "pins" ) {
    printPins();
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
  } else if( tcmd.path == "sht20/read" ) {
    char buf[64];
    TOGoS::BufferPrint bufPrn(buf, sizeof(buf));
    const TOGoS::SHT20::EverythingReading &reading = updateReading();
    if( reading.isValid() ) {
      bufPrn << "temp:" << reading.getTemperatureC() << "C" << " "
             << "humid:" << reading.getRhPercent() << "%";
      return CommandResult::ok(std::string(bufPrn.str()));
    } else {
      return CommandResult::failed("not connected");
    }
  } else if( tcmd.path == "help" ) {
    Serial << HENLO << "\n";
    Serial << "# \n";
    Serial << "# Commands:\n";
    Serial << "#   echo $arg1 .. $argN\n";
    Serial << "#   echo/lines $arg1 .. $argN\n";
    Serial << "#   status ; print status of WiFi, MQTT connection, etc\n";
    Serial << "#   pins ; print pin information\n";
    Serial << "#   wifi/connect $ssid $password ; connect to a WiFi network\n";
    Serial << "#   sht20/read ; Read values from SHT20\n";
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
  processEchoCommand,
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

void redrawStalenessBar() {
  int screenWidth = ssd1306.getColumnCount();
  ssd1306.gotoRowCol(1,0);
  long timeSincePreviousReading = millis() - latestReading.time;
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

void reportReading(uint8_t dest) {
  const TOGoS::SHT20::EverythingReading &reading = latestReading.data;
  publishAttr("sht20/reading/age", ((currentTime - latestReading.time) / 1000.0), PUB_SERIAL);
  publishAttr("sht20/connected", reading.isValid(), dest);
  if( reading.isValid() ) {
    publishAttr("sht20/temperature/c", reading.getTemperatureC(), dest);
    publishAttr("sht20/temperature/f", reading.getTemperatureF(), dest);
    publishAttr("sht20/humidity/percent", reading.getRhPercent(), dest);
  }
}

void redrawReading() {
  TOGoS::SSD1306::Printer printer(ssd1306, TOGoS::SSD1306::font8x8);

  // TODO: Put something on screen to show WiFi/MQTT connectivity

  ssd1306.gotoRowCol(2,0);
  printer << "Temperature:";
  printer.clearToEndOfRow();
  
  ssd1306.gotoRowCol(3,12);
  if( latestReading.data.temperature.isValid() ) {
    printer.print(latestReading.data.getTemperatureF(),1);
    printer << "F / ";
    printer.print(latestReading.data.getTemperatureC(),1);
    printer << "C";
  } else {
    printer << "----";
  }
  printer.clearToEndOfRow();
  
  ssd1306.gotoRowCol(4,0);
  printer << "Humidity:";
  printer.clearToEndOfRow();
  
  ssd1306.gotoRowCol(5,12);
  if( latestReading.data.humidity.isValid() ) {
    printer.print(latestReading.data.getRhPercent(),1);
    printer << "%";
  } else {
    printer << "----";
  }
  printer.clearToEndOfRow();
  
  ssd1306.gotoRowCol(6,0);
  printer << "VPD: ";
  if( latestReading.data.isValid() ) {
    printer.print(latestReading.data.getVpdKpa(),1);
    printer << " kPa";
  }
  printer.clearToEndOfRow();
  
  ssd1306.gotoRowCol(7,0);
  if( latestReading.data.isValid() ) {
    printer << "DP : ";
    printer.print(latestReading.data.getDewPoint().getF(),0);
    printer << " F";
  }
  printer.clearToEndOfRow();
}

// TODO: Make a class in TOGoSArduinoLibs that does this
struct WiFiCredz {
  const char *ssid;
  const char *password;
  WiFiCredz(const char *ssid, const char *password) : ssid(ssid), password(password) {}
};

std::vector<WiFiCredz> wifiNetworks;
int wifiNetworkIndex = -1;
unsigned long lastWifiReconnectAttempt = 0;

void wifiUpdate() {
  int status = WiFi.status();
  if( status != WL_CONNECTED && status != WL_IDLE_STATUS &&
      currentTime - lastWifiReconnectAttempt > 5000 && wifiNetworks.size() > 0 ) {
    ++wifiNetworkIndex;
    if( wifiNetworkIndex >= wifiNetworks.size() ) {
      wifiNetworkIndex = 0;
    }

    const WiFiCredz &credz = wifiNetworks[wifiNetworkIndex];
    Serial << "# Attempting auto-connect to " << credz.ssid << "...\n";
    WiFi.begin(credz.ssid, credz.password);
    lastWifiReconnectAttempt = currentTime;
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
  pinMode(D6, OUTPUT);
  pinMode(D7, OUTPUT);
  Serial << "\n\n"; // Get past any junk in the terminal
  Serial << "# " << APP_NAME << " setup()...\n";
  blinkBuiltin(BOOTING_BLINKS); // 5 blinks = booting, same as ArduinoPowerSwitch
  Serial << "# Initializing I2C...\n";
  i2c.begin();
  
  Serial << "# Initializing SHT20...\n";
  sht20.begin(TOGoS::SHT20::Driver::DEFAULT_ADDRESS);
  delay(100);

  Serial << "# Initializing SSD1306 (0x" << hexByte(SSD1306_ADDRESS) << ")...\n";
  ssd1306.begin(SSD1306_ADDRESS);
  Serial << "# And writing to it...\n";
  ssd1306.displayOn();
  ssd1306.setBrightness(255);
  drawBackground();
  
  Serial << "# We managed to not crash during boot!\n";
  Serial << "# Type 'help' for help.\n";

  latestReading.time    = millis() - 100000;
  latestMqttPublishTime = millis() - 100000;

#ifdef ES2021_POST_SETUP_CPP
  ES2021_POST_SETUP_CPP
#endif
}

unsigned long lastRedraw = 0;
unsigned long lastConnectionRedraw = 0;

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

  if( currentTime - latestReading.time > 15000 ) {
    updateReading();
  }

  if( currentTime - lastConnectionRedraw > 1000 ) {
    drawConnectionIndicators();
    lastConnectionRedraw = currentTime;
  }

  if( latestReading.time > lastRedraw ) {
    redrawReading();
    bool publishToMqtt = currentTime - latestMqttPublishTime > 15000;
    uint8_t pubDest = PUB_SERIAL;
    if( publishToMqtt && mqttMaintainer.isConnected() ) {
      pubDest |= PUB_MQTT;
      Serial << "# Publishing to " << getDefaultClientId() << "/...\n";
      latestMqttPublishTime = currentTime; // Assuming we really are connected lmao
    }
    reportReading(pubDest); // TODO: Maybe separate from redraw condition
    Serial << "\n";
    redrawStalenessBar();
    lastRedraw = currentTime;
  } else {
    redrawStalenessBar();
  }

  wifiUpdate();
}
