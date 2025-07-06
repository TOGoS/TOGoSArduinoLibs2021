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

#include <TOGoSStreamOperators.h>
#include <TOGoSSHT20.h>


// Two `TwoWire` instances, separate SCL
#define MSD_MODE_FOUR_WIRE 4
// Two `TwoWire` instances, shared SCL
#define MSD_MODE_THREE_WIRE 3
// One `TwoWire` instance, separate SCL
#define MSD_MODE_FOUR_WIRE_REINIT 2
// One `TwoWire` instance, shared SCL
#define MSD_MODE_THREE_WIRE_REINIT 1
// Only a single device
#define MSD_MODE_TWO_WIRE 0
// Use local variables for I2C and SHT20 driver
#define MSD_MODE_THREE_WIRE_LOCAL 5

// Another possible approach would be to keep the I2C and SHT20 driver
// instances in local variables, use them and forget them

//// Configuration

#define MSD_MODE MSD_MODE_THREE_WIRE_LOCAL
const char *APP_NAME = "MultiSHTDemo";

////

#if MSD_MODE == MSD_MODE_FOUR_WIRE
#define MSD_I2CA_SCL D1
#define MSD_I2CA_SDA D2
#define MSD_I2CB_SCL D3
#define MSD_I2CB_SDA D4
#define MSD_SEPARATE_INSTANCES 1
#define MSD_REINIT 0
#define MSD_GLOBAL_DRIVERS 1
#define MSD_LOCAL_DRIVERS 0
#endif

#if MSD_MODE == MSD_MODE_THREE_WIRE
#define MSD_I2CA_SCL D1
#define MSD_I2CA_SDA D2
#define MSD_I2CB_SCL D1
#define MSD_I2CB_SDA D4
#define MSD_SEPARATE_INSTANCES 1
#define MSD_REINIT 0
#define MSD_GLOBAL_DRIVERS 1
#define MSD_LOCAL_DRIVERS 0
#endif

#if MSD_MODE == MSD_MODE_FOUR_WIRE_REINIT
#define MSD_I2CA_SCL D1
#define MSD_I2CA_SDA D2
#define MSD_I2CB_SCL D3
#define MSD_I2CB_SDA D4
#define MSD_SEPARATE_INSTANCES 0
#define MSD_REINIT 1
#define MSD_GLOBAL_DRIVERS 1
#define MSD_LOCAL_DRIVERS 0
#endif

#if MSD_MODE == MSD_MODE_THREE_WIRE_REINIT
#define MSD_I2CA_SCL D1
#define MSD_I2CA_SDA D2
#define MSD_I2CB_SCL D1
#define MSD_I2CB_SDA D4
#define MSD_SEPARATE_INSTANCES 0
#define MSD_REINIT 1
#define MSD_GLOBAL_DRIVERS 1
#define MSD_LOCAL_DRIVERS 0
#endif

#if MSD_MODE == MSD_MODE_TWO_WIRE
#define MSD_I2CA_SCL D1
#define MSD_I2CA_SDA D2
#define MSD_SEPARATE_INSTANCES 0
#define MSD_REINIT 0
#define MSD_GLOBAL_DRIVERS 1
#define MSD_LOCAL_DRIVERS 0
#endif

#if MSD_MODE == MSD_MODE_THREE_WIRE_LOCAL
#define MSD_I2CA_SCL D1
#define MSD_I2CA_SDA D2
#define MSD_I2CB_SCL D1
#define MSD_I2CB_SDA D4
#define MSD_SEPARATE_INSTANCES 0
#define MSD_REINIT 1
#define MSD_GLOBAL_DRIVERS 0
#define MSD_LOCAL_DRIVERS 1
#endif


#if MSD_GLOBAL_DRIVERS == 1
TwoWire i2cA;
#if MSD_SEPARATE_INSTANCES == 1
TwoWire i2cB;
#endif
TOGoS::SHT20::Driver sht20A(i2cA);
#if MSD_SEPARATE_INSTANCES == 1
TOGoS::SHT20::Driver sht20B(i2cB);
#endif
#endif

// Note: For a less barebones demo, I would return the reading
// and report separately, later.

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

void initReadAndReport(const char *name, int sda, int scl, Print &out) {
  TwoWire i2c;
  TOGoS::SHT20::Driver sht20(i2c);
  i2c.begin(sda, scl);
  //delay(100)
  sht20.begin(TOGoS::SHT20::Driver::DEFAULT_ADDRESS);
  //delay(100)
  readAndReport(name, sht20, out);
}

void setup() {

  delay(500); // Standard 'give me time to reprogram it' delay in case the program messes up some I/O pins
  Serial.begin(115200);
  pinMode(D5, OUTPUT); // Provides I2C GND

  Serial << "\n\n"; // Get past any junk in the terminal
  Serial << "# " << APP_NAME << " setup()...\n";
  Serial << "# Initializing I2C...\n";

#if MSD_GLOBAL_DRIVERS == 1
  i2cA.begin(MSD_I2CA_SDA,MSD_I2CA_SCL);
  delay(100); // Just in case
#if MSD_SEPARATE_INSTANCES == 1
  i2cA.begin(MSD_I2CB_SDA,MSD_I2CB_SCL);
  delay(100); // Just in case
#endif

  Serial << "# Initializing SHT20A...\n";
  sht20A.begin(TOGoS::SHT20::Driver::DEFAULT_ADDRESS);
  delay(100); // Just in case

#if MSD_SEPARATE_INSTANCES == 1
  Serial << "# Initializing SHT20B...\n";
  sht20B.begin(TOGoS::SHT20::Driver::DEFAULT_ADDRESS);
  delay(100); // Just in case
#endif
#endif
}

void loop() {
  Serial << "# Loop\n";
  Serial << "# MSD_MODE = " << MSD_MODE << "\n";
  Serial << "# MSD_REINIT = " << MSD_REINIT << "\n";
  Serial << "# MSD_SEPARATE_INSTANCES = " << MSD_SEPARATE_INSTANCES << "\n";

#if MSD_LOCAL_DRIVERS == 1
  initReadAndReport("sht20-a", MSD_I2CA_SDA, MSD_I2CA_SCL, Serial);
  initReadAndReport("sht20-b", MSD_I2CB_SDA, MSD_I2CB_SCL, Serial);
#else
#if MSD_REINIT == 1
  i2cA.begin(MSD_I2CA_SDA,MSD_I2CA_SCL);
  sht20A.begin(TOGoS::SHT20::Driver::DEFAULT_ADDRESS);
  delay(100);
#endif
  readAndReport("sht20-a", sht20A, Serial);
  
#if MSD_SEPARATE_INSTANCES == 1
  readAndReport("sht20-b", sht20B, Serial);
#elif MSD_REINIT == 1
  i2cA.begin(MSD_I2CB_SDA,MSD_I2CB_SCL);
  sht20A.begin(TOGoS::SHT20::Driver::DEFAULT_ADDRESS);
  delay(100);
  readAndReport("sht20-b", sht20A, Serial);
#endif
#endif
  delay(1500);
}
