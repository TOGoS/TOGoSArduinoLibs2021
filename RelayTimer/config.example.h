// D1 is used by https://www.wemos.cc/en/latest/d1_mini_shield/relay.html

constexpr TOGoS::Arduino::RelayTimer::AppConfig appConfig = {
	.appName = "RelayTimer",
	.relayControlPin = D1,
	.relayIsActiveLow = true,
	.buttonPin = D7,
	.buttonIsActiveLow = true,
};
