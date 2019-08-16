#include <DS3231.h>
#include <Wire.h>

DS3231 Clock;

void setup()
{
  Serial.begin(9600);

  Wire.begin();

  setDatetime(0, 0, 0, 0, 0, 0);
}

void loop()
{
  delay(1000);
  printDatetime();
}

void printDatetime()
{
  bool Century=false;
  bool h12 = false;
  bool PM = false;

  Serial.print(Clock.getYear(), DEC);
  Serial.print("-");
  Serial.print(Clock.getMonth(Century), DEC);
  Serial.print("-");
  Serial.print(Clock.getDate(), DEC);
  Serial.print(" ");
  Serial.print(Clock.getHour(h12, PM), DEC);
  Serial.print(":");
  Serial.print(Clock.getMinute(), DEC);
  Serial.print(":");
  Serial.println(Clock.getSecond(), DEC);
}

void setDatetime(int Year, byte Month, byte Day, byte Hour, byte Minute, byte Second)
{
  Clock.setClockMode(false);
  Clock.setYear(Year);
  Clock.setMonth(Month);
  Clock.setDate(Day);
  Clock.setHour(Hour);
  Clock.setMinute(Minute);
  Clock.setSecond(Second);
}
