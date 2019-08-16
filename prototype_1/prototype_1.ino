#include <HX711.h>
#include <LiquidCrystal.h>

#define RS_PIN 12
#define EN_PIN 11
#define D4_PIN 5
#define D5_PIN 4
#define D6_PIN 3
#define D7_PIN 2
#define DT_PIN 7
#define SCK_PIN 6

LiquidCrystal lcd(RS_PIN, EN_PIN, D4_PIN, D5_PIN, D6_PIN, D7_PIN);
HX711 scale;

const float scale_calibration_factor = 21500.0, scale_zero_factor = 0.35;
char scale_weight[5];
char lcd_line0[16], lcd_line1[16];

void setup()
{
  // set up the LCD
  lcd.begin(16, 2); // columns, rows
  lcd.clear();

  // set up scale
  scale.begin(DT_PIN, SCK_PIN);
  scale.set_scale(scale_calibration_factor);
}

void loop()
{
  // read weight from scale and display it on the LCD
  delay(1000);
  dtostrf(fabs(scale.get_units(3) - scale_zero_factor), 3, 1, scale_weight);
  sprintf(lcd_line0, "    %-4s Kg", scale_weight);
  updateDisplay();
}

void updateDisplay()
{
   lcd.setCursor(0,0);
   lcd.print(lcd_line0);
   lcd.print(lcd_line1);
}
