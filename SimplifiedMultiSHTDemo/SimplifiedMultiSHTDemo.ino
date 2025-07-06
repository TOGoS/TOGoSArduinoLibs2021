// tab-width:2 mode:c++
//
// SimplifiedMultiSHTDemo
// 
// Demonstrates talking to multiple I2C busses (which may share an SCL pin)
// by reinitializing the `Wire` object between reads
// 
// Maybe should be an example in TOGoSSHT20 library?

#include <TOGoSStreamOperators.h>
#include <TOGoSSHT20.h>

// Another possible approach would be to keep the I2C and SHT20 driver
// instances in local variables, use them and forget them

//// Begin configuration section

const char *APP_NAME = "SimplifiedMultiSHTDemo";
#define MSD_I2CA_SCL D1
#define MSD_I2CA_SDA D2
// Set to D1 to hare SCL between the two sensors, D3 to give it its own SCL
#define MSD_I2CB_SCL D1
#define MSD_I2CB_SDA D4

// Delay between i2c and sht20 begin() calls, to "give electrons time to settle"?
#define MSD_STEP_DELAY 50
// Target interval of loop runs
#define MSD_LOOP_INTERVAL 1500

//// End configuration section

// Note: For a less barebones demo, I would return the reading
// and report separately, later.

void readAndReport(const char *name, TOGoS::SHT20::Driver &sht20, Print &out) {
  TOGoS::SHT20::EverythingReading data = sht20.readEverything();
  if( data.isValid() ) {
    out << name << "/connected:100, ";
    out << name << "/temperature/f:" << data.getTemperatureF() << ", ";
    out << name << "/temperature/c:" << data.getTemperatureC() << ", ";
    out << name << "/humidity/percent:" << data.getRhPercent() << ", ";
  } else {
    out << name << "/connected:0, ";
  }
}

long nextLoopTime;
void setup() {
  delay(500); // Standard 'give me time to reprogram it' delay in case the program messes up some I/O pins
  Serial.begin(115200);
  pinMode(D5, OUTPUT); // Provides I2C GND
  nextLoopTime = millis();
}

void loop() {
  long delayTime = nextLoopTime - millis();
  nextLoopTime += MSD_LOOP_INTERVAL;
  
  if( delayTime > 0 ) delay(delayTime);
  
  {
    TOGoS::SHT20::Driver sht20(Wire);
    
    Wire.begin(MSD_I2CA_SDA, MSD_I2CA_SCL);
    delay(MSD_STEP_DELAY);
    sht20.begin(TOGoS::SHT20::Driver::DEFAULT_ADDRESS);
    delay(MSD_STEP_DELAY);
    readAndReport("sht20-a", sht20, Serial);
    
    Wire.begin(MSD_I2CB_SDA, MSD_I2CB_SCL);
    delay(MSD_STEP_DELAY);
    sht20.begin(TOGoS::SHT20::Driver::DEFAULT_ADDRESS);
    delay(MSD_STEP_DELAY);
    readAndReport("sht20-b", sht20, Serial);
    
    Serial << "\n";
  }
}
