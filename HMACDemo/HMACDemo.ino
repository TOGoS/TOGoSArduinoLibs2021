#include <string>
#include <Arduino.h>
#include <Hash.h>
#include <TOGoSStreamOperators.h>

char hexDigit(int num) {
  num = num & 0xF;
  if( num < 10 ) return '0' + num;
  if( num < 16 ) return 'a' + num - 10;
  return '?'; // Should be unpossible
}

std::string toHex(const uint8_t *data, size_t len) {
  std::string hecks;
  for( off_t i = 0; i < len; ++i ) {
    hecks += hexDigit(data[i] >> 4);
    hecks += hexDigit(data[i]);
  }
  return hecks;
}

// Based on pseudocode from https://en.wikipedia.org/wiki/HMAC
void hmacSha1(const uint8_t *key, size_t keyLen, const uint8_t *message, size_t messageLen, uint8_t *dest) {
	// The built-in SHA1 function
	// can only do a whole dang message!
	// So I guess we allocate a giant buffer...
	// 
	// TODO: Variant for case where message happens to have a hashSize buffer before it!
	// 
	// TODO: Alternatively, implement SHA-1 that can be given chunks.
	// Could base this on https://github.com/TOGoS/TSHash/blob/master/src/main/ts/tshash/SHA1.ts.
	const size_t hmacBufLen = 1536;
	uint8_t hmacBuf[hmacBufLen]; // Holds iKeyPad ++ message
	
	const size_t blockSize = 64;
	const size_t hashSize = 20;
	size_t padPlusMessageLen = blockSize + messageLen;

	if( padPlusMessageLen > hmacBufLen ) {
		// Can't do it!  Return all zeroes to indicate failure. :-P
		for( off_t i=0; i<hashSize; ++i ) {
			dest[i] = 0;
		}
		return;
	}

	uint8_t oKeyPad[blockSize + hashSize]; // Also used for final hash
	uint8_t blockKey[blockSize];
	
	if( keyLen > blockSize ) {
		sha1(key, keyLen, blockKey);
		for( off_t i=hashSize; i<blockSize; ++i ) {
			blockKey[i] = 0;
		}
	} else {
		for( off_t i=0; i<keyLen; ++i ) {
			blockKey[i] = key[i];
		}
		for( off_t i=keyLen; i<blockSize; ++i ) {
			blockKey[i] = 0;
		}
	}
	
	for( off_t i=0; i<blockSize; ++i ) {
		oKeyPad[i] = blockKey[i] ^ 0x5c;
		hmacBuf[i] = blockKey[i] ^ 0x36;
	}
	for( off_t i=0; i<messageLen; ++i ) {
		hmacBuf[blockSize + i] = message[i];
	}
	
	sha1(hmacBuf, padPlusMessageLen, oKeyPad + blockSize);
	sha1(oKeyPad, blockSize + hashSize, dest);
}

void setup() {
	uint8_t sha1Buf[20];
	const char *secret = "foo";
	const char *message = "Hello, world!";
	
	Serial.begin(115200);
	sha1(message, strlen(message), sha1Buf);
	
	delay(2000);
	
	Serial << "\n";
	Serial << "hex(sha1(\"" << message << "\")): ";
	Serial << toHex(sha1Buf, sizeof(sha1Buf)).c_str();
	Serial << "\n";
	
	hmacSha1((const uint8_t *)secret, strlen(secret), (const uint8_t *)message, strlen(message), sha1Buf);

	Serial << "hex(hmacSha1(\"" << secret << "\", \"" << message << "\")): ";
	Serial << toHex(sha1Buf, sizeof(sha1Buf)).c_str();
	Serial << "\n";
}

void loop() {
	delay(1000);
}
