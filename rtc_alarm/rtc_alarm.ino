#include <Wire.h>
#include <DS3231.h>
#include <LowPower.h>

#define WAKE_UP_PIN 2

DS3231 Clock;

int Year = 0;
byte Month = 0;
byte Day = 0;
byte Hour = 0;
byte Minute = 0;
byte Second = 0;

void setup()
{
  Serial.begin(9600);

  Wire.begin(); // Start the I2C interface

  // Configure Interrupt Pin
  pinMode(WAKE_UP_PIN, INPUT_PULLUP);
  digitalWrite(WAKE_UP_PIN, HIGH);
  attachInterrupt(digitalPinToInterrupt(WAKE_UP_PIN), wakeUp, FALLING);

  setAlarmAndPowerDown();
}

void loop()
{
  printDatetime();
  
  blinkLED();

  setAlarmAndPowerDown();
}

void setAlarmAndPowerDown()
{
  Serial.println("Setting alarm to 10 seconds and powering down...");
  resetDatetime();
  setAlarm();
  delay(1000);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
}

void setAlarm()
{
  Clock.setA1Time(Day, Hour, Minute, Second + 10, 0x0, false, false, false);

  Clock.turnOnAlarm(1);
  Clock.turnOffAlarm(2);
  Clock.checkIfAlarm(1);
  Clock.checkIfAlarm(2);
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

void resetDatetime()
{
  Clock.setClockMode(false);
  Clock.setYear(Year);
  Clock.setMonth(Month);
  Clock.setDate(Day);
  Clock.setHour(Hour);
  Clock.setMinute(Minute);
  Clock.setSecond(Second);
}

void blinkLED()
{
  digitalWrite(LED_BUILTIN, HIGH);
  delay(5000);
}

void wakeUp()
{
  Serial.print("Woke up, time is ");
}
