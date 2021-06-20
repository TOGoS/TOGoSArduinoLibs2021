#include "TOGoSSHT20.h"
#include <TOGoSStreamOperators.h>
TOGoS::SHT20 sht20;

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  sht20.begin();
}

void loop()
{
  sht20.measure_all();
  Serial << "# Hello from TOGoSSHT20/examples/MeasureEverything\n";
  Serial << "# " << sht20.tempC << "°C\n";
  Serial << "# " << sht20.tempF << "°F\n";
  Serial << "# " << sht20.dew_pointC << "°C dew point\n";
  Serial << "# " << sht20.dew_pointF << "°F dew point\n";
  Serial << "# " << sht20.RH << " %RH\n";
  Serial << "# " << sht20.vpd() << " kPa VPD\n";
  Serial << "\n";
  delay(5000);
}

/*
Original output:

24.72°C
76.49°F
23.93°C dew point
75.08°F dew point
95.38 %RH
0.14 kPa VPD

[22:30]
24.84°C
76.71°F
24.19°C dew point
75.54°F dew point
96.20 %RH
0.12 kPa VPD
*/
