#include <TOGoSSHT20.h>
#include <TOGoSStreamOperators.h>

TOGoS::SHT20::Driver sht20(Wire);

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  sht20.begin();
}

void loop()
{
  TOGoS::SHT20::EverythingReading reading = sht20.readEverything();
  Serial << "# Hello from TOGoSSHT20/examples/MeasureEverything\n";
  Serial << "# " << reading.getTemperature().getC() << "°C\n";
  Serial << "# " << reading.getTemperature().getF() << "°F\n";
  Serial << "# " << reading.getDewPoint().getC() << "°C dew point\n";
  Serial << "# " << reading.getDewPoint().getF() << "°F dew point\n";
  Serial << "# " << reading.getRhPercent() << " %RH\n";
  Serial << "# " << reading.getVpdKpa() << " kPa VPD\n";
  Serial << "\n";

  sht20.measure_all();

  Serial << "# Readings from original code:\n";
  Serial << "# " << sht20.tempC << "°C\n";
  Serial << "# " << sht20.tempF << "°F\n";
  Serial << "# " << sht20.dew_pointC << "°C dew point\n";
  Serial << "# " << sht20.dew_pointF << "°F dew point\n";
  Serial << "# " << sht20.RH << " %RH\n";
  Serial << "# " << sht20.vpd_kPa << " kPa VPD\n";
  Serial << "\n";
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
