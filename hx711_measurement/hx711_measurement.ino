#include <HX711.h>

#define DT_PIN  7
#define SCK_PIN  6

HX711 scale;

const float calibration_factor = 21500.0; // 21500 for S/N 18082920837, 20750 for S/N 18080902727

void setup()
{
  Serial.begin(9600);
  Serial.println("HX711 scale demo");

  scale.begin(DT_PIN, SCK_PIN);
  scale.set_scale(calibration_factor); // see hx711_calibration sketch
  scale.tare();

  Serial.println("Readings:");
}

void loop()
{
  Serial.print("Reading: ");
  Serial.print(scale.get_units(), 1); // scale.get_units() returns a float
  Serial.print(" kg");
  Serial.println();
}
