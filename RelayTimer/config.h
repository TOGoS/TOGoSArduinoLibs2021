// D1 is used by https://www.wemos.cc/en/latest/d1_mini_shield/relay.html

// TAA_RELAYTIMER_COARSE_VERSION should have been set by the main program

#include <TOGoSPreprocFun.h>
#include <TOGoS/PreprocFun/ConfigHash.h>

// When you create a secrets file, use `git hash-object` to name it,
// then change this to reflect that hash.
// e69de29bb2d1d6434b8b29ae775ad8c2e48c5391 is the empty file.
#define TAA_RELAYTIMER_SECRETS_FILE_HASH e69de29bb2d1d6434b8b29ae775ad8c2e48c5391

#include "version.h"
#include "commit-hash.h"

#ifdef TAA_RELAYTIMER_SECRETS_FILE_HASH
#define TAA_RELAYTIMER_SECRETS_INCLUDESPEC TOGOSARDUINOAPPS2021_CONFIG_HASH_TO_INCLUDESPEC(secrets,TAA_RELAYTIMER_SECRETS_FILE_HASH)
#include TAA_RELAYTIMER_SECRETS_INCLUDESPEC
#endif

constexpr TOGoS::Arduino::RelayTimer::AppConfig appConfig = {
	.appName = "RelayTimer",
	// Since this config.h is included in the repo,
	// its commit hash will transitively include TAA_RELAYTIMER_SECRETS_FILE_HASH.
	.appVersion = "v" TAA_RELAYTIMER_COARSE_VERSION "-" TAA_RELAYTIMER_COMMIT_HASH,
	.relayControlPin = D1,
	.relayIsActiveLow = true,
	.buttonPin = D7,
	.buttonIsActiveLow = true,
	// ONE_SHOT or LOOPING
	.timerMode = TOGoS::Arduino::RelayTimer::ONE_SHOT,
	.oneShotConfig {
		//
		.shortPressTimerIncrement = 1000*3600
	},
	//	.loopingConfig {
	//	.onDuration = 100,
	//	.loopDuration = 1000*3600*24,
	//}
};
