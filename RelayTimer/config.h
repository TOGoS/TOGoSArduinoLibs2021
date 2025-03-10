// D1 is used by https://www.wemos.cc/en/latest/d1_mini_shield/relay.html

constexpr TOGoS::Arduino::RelayTimer::AppConfig appConfig = {
	.appName = "Asd123",
	.relayControlPin = D1,
	.relayIsActiveLow = false,
	.buttonPin = D7,
	.buttonIsActiveLow = true,
	.timerMode = TOGoS::Arduino::RelayTimer::LOOPING,
};
