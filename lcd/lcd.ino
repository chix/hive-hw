#include <LiquidCrystal.h>

#define RS_PIN 12
#define EN_PIN 11
#define D4_PIN 5
#define D5_PIN 4
#define D6_PIN 3
#define D7_PIN 2

LiquidCrystal lcd(RS_PIN, EN_PIN, D4_PIN, D5_PIN, D6_PIN, D7_PIN);

void setup()
{
  lcd.begin(16, 2); // columns, rows
  lcd.print("Hello");
}

void loop()
{
  lcd.noDisplay();
  delay(500);
  lcd.display();
  delay(500);
}
