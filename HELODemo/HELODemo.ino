#include <ESP8266WiFi.h>
#include <AddrList.h>
#include <WiFiUdp.h>

const char *myHostname = "helodemo";
const byte myIp4[] = {10, 9, 254, 254};
const byte myIp4Gateway[] = {10, 9, 254, 254};
const byte myIp4Subnet[] = {255, 255, 255, 255};

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

void setup() {
	IPAddress ip4 = myIp4;
	IPAddress ip4Gateway = myIp4Gateway;
	IPAddress ip4Subnet = myIp4Subnet;

	Serial.begin(115200);
	Serial.println();
	delay(1000);
	Serial.println(F("Hello"));

	// The regular Arduino WiFi can just take the IP address,
	// but the ESP8266WiFi needs at least these three arguments:
	WiFi.config(ip4, ip4Gateway, ip4Subnet);
	WiFi.hostname(myHostname); // This needs to come after `config`
	WiFi.begin(); // And finally, after config and hostname, you may begin.
}

void loop() {
	printStatus(Serial);

	delay(1000);
}
