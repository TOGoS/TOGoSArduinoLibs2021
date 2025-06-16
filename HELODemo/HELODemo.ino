#include <ESP8266WiFi.h>
#include <AddrList.h>
#include <WiFiUdp.h>
#include <TOGoSBufferPrint.h>
#include <TOGoSStreamOperators.h>

const char *myHostname = "helodemo";
const byte myIp4[] = {10, 9, 254, 254};
const byte myIp4Gateway[] = {10, 9, 254, 254};
const byte myIp4Subnet[] = {255, 255, 255, 255};
const int myUdpPort = 16378;

char hexDigit(int num) {
  num = num & 0xF;
  if( num < 10 ) return '0' + num;
  if( num < 16 ) return 'A' + num - 10;
  return '?'; // Should be unpossible
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

void printStatus(Print &out) {
	out.println(F("--------------"));
#if LWIP_IPV6
	out.printf("IPV6 is enabled\n");
#else
	out.printf("IPV6 is not enabled\n");
#endif

	for (auto a : addrList) {
		out.printf("IF='%s' IPv6=%d local=%d hostname='%s' addr= %s",
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
		
		out.println();
	}
}


WiFiUDP udp;

void setup() {
	IPAddress ip4 = myIp4;
	IPAddress ip4Gateway = myIp4Gateway;
	IPAddress ip4Subnet = myIp4Subnet;

	Serial.begin(115200);
	Serial.println();
	delay(1000);
	Serial.println(F("Hello"));
	
	// Note that we're not indicating the SSID/password, here.
	// I think these ESP8266 boards remember whatever it was last set to,
	// so it doesn't have to be baked into the program.
	
	// The regular Arduino WiFi can just take the IP address,
	// but the ESP8266WiFi needs at least these three arguments:
	WiFi.config(ip4, ip4Gateway, ip4Subnet);
	WiFi.hostname(myHostname); // This needs to come after `config`
	WiFi.begin(); // And finally, after config and hostname, you may begin.
	
	udp.begin(myUdpPort);
}

long lastBroadcast = -1;

void loop() {
	long currentTime = millis();
	long rand = random(0, 4000000000l);
	
	printStatus(Serial);
	
	if( currentTime - lastBroadcast > 10000 ) {
		byte macAddressBuffer[6];
		WiFi.macAddress(macAddressBuffer);
		
		char buf[1024];
		TOGoS::BufferPrint bufPrn(buf, sizeof(buf));
		
		
		//bufPrn << "#HELO //" << macAddressToHex(macAddressBuffer, "-") << "/\n";
		bufPrn << "#HELO\n";
		bufPrn << "\n";
	   bufPrn << "mac " << macAddressToHex(macAddressBuffer, ":") << "\n";
		bufPrn << "clock " << currentTime << "\n";
		bufPrn << "rand " << rand << "\n";

		Serial.println("Broadcasting a packet");
		
		udp.beginPacket("ff02::1", myUdpPort);
		udp.write(buf, bufPrn.size());
		udp.endPacket();
		
		lastBroadcast = currentTime;
	}

	delay(1000);
}
