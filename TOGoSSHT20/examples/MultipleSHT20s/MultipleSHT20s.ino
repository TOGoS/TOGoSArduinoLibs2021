// MultipleSHT20s
// 
// Demonstrates talking to multiple SHT20s (which may share an SCL pin)
// by reinitializing the `Wire` object between reads to use
// different I2C busses, sharing SCL or not (see configuration section)

#include <TOGoSSHT20.h>

// Another possible approach would be to keep the I2C and SHT20 driver
// instances in local variables, use them and forget them

//// Begin configuration section

#define APP_NAME "MultipleSHT20s"
#define MSD_I2C0_SCL D1
#define MSD_I2C0_SDA D2
// Set to D1 to hare SCL between the two sensors, D3 to give it its own SCL
#define MSD_I2C1_SCL D1
#define MSD_I2C1_SDA D3
#define MSD_I2C2_SCL D1
#define MSD_I2C2_SDA D4
#define MSD_I2C3_SCL D1
#define MSD_I2C3_SDA D7

// Delay between i2c and sht20 begin() calls, to "give electrons time to settle"?
#define MSD_STEP_DELAY 50
// Target interval of loop runs
#define MSD_LOOP_INTERVAL 1500

//// End configuration section

template <typename T>
void printProp(const char *devName, const char *propName, const T &value, Print &out) {
  // Of course if you #include <TOGoSStreamOperators.h> this can all look a little nicer:
  out.print(devName);
  out.print("/");
  out.print(propName);
  out.print(":");
  out.print(value);
  out.print(", ");
}

void readAndReport(const char *devName, TOGoS::SHT20::Driver &sht20, Print &out) {
  TOGoS::SHT20::EverythingReading data = sht20.readEverything();
  // Words abbreviated so they fit on your screen better
  if( data.isValid() ) {
    printProp(devName, "conn" ,                100    , out);
    printProp(devName, "t/f"  , data.getTemperatureF(), out);
    printProp(devName, "t/c"  , data.getTemperatureC(), out);
    printProp(devName, "h/pct", data.getRhPercent()   , out);
  } else {
    printProp(devName, "conna",                  0    , out);
  }
}

long nextLoopTime;
void setup() {
  delay(500); // Standard 'give me time to reprogram it' delay in case the program messes up some I/O pins
  Serial.begin(115200);
  pinMode(D5, OUTPUT); // Provides I2C GND
  nextLoopTime = millis();
  
  Serial.print("\n\n");
  Serial.print("# Hello from "); Serial.print(APP_NAME); Serial.print("!\n");
  Serial.print("\n\n");
  delay(1000);
}

void loop() {
  long delayTime = nextLoopTime - millis();
  nextLoopTime += MSD_LOOP_INTERVAL;
  
  if( delayTime > 0 ) delay(delayTime);
  
  {
    TOGoS::SHT20::Driver sht20(Wire);
    
#ifdef MSD_I2C0_SDA
    Wire.begin(MSD_I2C0_SDA, MSD_I2C0_SCL);
    delay(MSD_STEP_DELAY);
    sht20.begin(TOGoS::SHT20::Driver::DEFAULT_ADDRESS);
    delay(MSD_STEP_DELAY);
    readAndReport("temhum0", sht20, Serial);
#endif
    
#ifdef MSD_I2C1_SDA
    Wire.begin(MSD_I2C1_SDA, MSD_I2C1_SCL);
    delay(MSD_STEP_DELAY);
    sht20.begin(TOGoS::SHT20::Driver::DEFAULT_ADDRESS);
    delay(MSD_STEP_DELAY);
    readAndReport("temhum1", sht20, Serial);
#endif

#ifdef MSD_I2C2_SDA
    Wire.begin(MSD_I2C2_SDA, MSD_I2C2_SCL);
    delay(MSD_STEP_DELAY);
    sht20.begin(TOGoS::SHT20::Driver::DEFAULT_ADDRESS);
    delay(MSD_STEP_DELAY);
    readAndReport("temhum2", sht20, Serial);
#endif

#ifdef MSD_I2C3_SDA
    Wire.begin(MSD_I2C3_SDA, MSD_I2C3_SCL);
    delay(MSD_STEP_DELAY);
    sht20.begin(TOGoS::SHT20::Driver::DEFAULT_ADDRESS);
    delay(MSD_STEP_DELAY);
    readAndReport("temhum3", sht20, Serial);
#endif
    
    Serial.print("\n");
  }
}
