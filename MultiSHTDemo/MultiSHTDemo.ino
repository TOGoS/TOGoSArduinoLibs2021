// tab-width:2 mode:c++
//
// MultiSHTDemo
// 
// Minimal sketch for trying to read from more than one I2C SHT20,
// because apparently this is tricky.
// 
// TODO: Try sharing a pin
// 
// TODO: Try having just *one* TwoWire instance, and re-initialize it
// with different pins to read different sensor

const char *APP_NAME = "EnvironmentalSensor2021";


#include <TOGoSStreamOperators.h>
#include <TOGoSSHT20.h>

TwoWire i2cA;
TwoWire i2cB;
TOGoS::SHT20::Driver sht20A(i2cA);
TOGoS::SHT20::Driver sht20B(i2cB);

void readAndReport(const char *name, TOGoS::SHT20::Driver &sht20, Print &out) {
  TOGoS::SHT20::EverythingReading data = sht20.readEverything();
  if( data.isValid() ) {
    out << name << ": apparently connected\n";
    out << "  temperature/f: " << data.getTemperatureF() << "\n";
    out << "  temperature/c: " << data.getTemperatureC() << "\n";
    out << "  humidity/percent: " << data.getRhPercent() << "\n";
  } else {
    out << name << ": invalid reading\n";
  }
}

void setup() {
  delay(500); // Standard 'give me time to reprogram it' delay in case the program messes up some I/O pins
  Serial.begin(115200);
  
  Serial << "\n\n"; // Get past any junk in the terminal
  Serial << "# " << APP_NAME << " setup()...\n";
  Serial << "# Initializing I2C...\n";
  
  i2cA.begin(D2,D1);
  delay(100); // Just in case
  i2cB.begin(D4,D3);
  delay(100); // Just in case

  Serial << "# Initializing SHT20A...\n";
  sht20A.begin(TOGoS::SHT20::Driver::DEFAULT_ADDRESS);
  delay(100); // Just in case
  
  Serial << "# Initializing SHT20B...\n";
  sht20B.begin(TOGoS::SHT20::Driver::DEFAULT_ADDRESS);
  delay(100); // Just in case
}

void loop() {
  readAndReport("sht20-a", sht20A, Serial);
  readAndReport("sht20-b", sht20B, Serial);
  delay(500);
}
