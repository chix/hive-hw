#include <DS3231.h>
#include <HX711.h>
#include <LowPower.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <SPI.h>
#include <Wire.h>

#define WAKE_UP_PIN 2
#define SCK_PIN 6
#define DT_PIN 7
#define CE_PIN 9
#define CSN_PIN 10

DS3231 Clock;
HX711 scale;
RF24 radio(CE_PIN, CSN_PIN);

const bool debug = true;
const byte thisSlaveAddress[5] = {'H','0','0','0','2'}, confirmationResponse = 1;
char cmd, lastCmd;
float reading = 0;
unsigned long currentMillis = 0, prevMillis = 0, maxLoopMillis = 59000;
const float scale_calibration_factor = 20750.0, scale_zero_factor = -0.5;
int Year, Month, Day, Hour, Minute, Second = 0;
int alarmHours = 0, alarmMinutes = 0, alarmSeconds = maxLoopMillis / 1000;
bool wokeUp = false, confirmationResponseLoaded = true;

void setup()
{
  byte i;
  
  if (debug) {
    Serial.begin(9600);
  }

  // Start the I2C interface
  Wire.begin();

  // Configure Interrupt Pin
  pinMode(WAKE_UP_PIN, INPUT_PULLUP);
  digitalWrite(WAKE_UP_PIN, HIGH);
  attachInterrupt(digitalPinToInterrupt(WAKE_UP_PIN), wakeUp, FALLING);

  // set up scale
  scale.begin(DT_PIN, SCK_PIN);
  scale.set_scale(scale_calibration_factor);

  // set up radio rx
  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.openReadingPipe(1, thisSlaveAddress);
  radio.setAutoAck(true);
  radio.enableAckPayload();
  radio.startListening();
  loadConfirmationResponse();

  if (debug) {
    for (i = 0; i < 5; i++) { Serial.print(char(thisSlaveAddress[i])); }
    Serial.println(": waiting for commands");
  }
}

void loop()
{
  if (wokeUp) {
    prevMillis = millis();
    currentMillis = millis();
    lastCmd = ' ';
    reading = 0;
    radio.powerUp();
    radio.begin();
    radio.setDataRate(RF24_250KBPS);
    radio.openReadingPipe(1, thisSlaveAddress);
    radio.setAutoAck(true);
    radio.enableAckPayload();
    radio.startListening();
    loadConfirmationResponse();
    wokeUp = false;
  }

  readCommand();

  switch(cmd) {
    case 'R':
      if (debug) { printResponse(); }
      loadReadingResponse();
      lastCmd = 'R';
      break;
    case 'S':
      if (debug) { printResponse(); }
      loadConfirmationResponse();
      lastCmd = 'S';
      break;
    case 'X': // execute
      if (debug) { printResponse(); }
      loadConfirmationResponse();
      if (lastCmd == 'R') {
        lastCmd = ' ';
      } else if (lastCmd == 'S') {
        lastCmd = ' ';
        setAlarmAndPowerDown();
      }
      break;
  }

  currentMillis = millis();
  if ((currentMillis - prevMillis) >= maxLoopMillis) {
    if (debug) { Serial.println("Max loop time reached"); }
    setAlarmAndPowerDown();
  }
}

void readCommand()
{
  cmd = ' ';
  if (radio.available()) {
    radio.read(&cmd, sizeof(cmd));
    if (debug) {
      Serial.print(cmd);
      Serial.print(": ");
    }
  }
}

void loadReadingResponse()
{
  reading = fabs(scale.get_units(1) - scale_zero_factor);
  radio.writeAckPayload(1, &reading, sizeof(reading));
  confirmationResponseLoaded = false;
}

void loadConfirmationResponse()
{
  radio.writeAckPayload(1, &confirmationResponse, sizeof(confirmationResponse));
  confirmationResponseLoaded = true;
}

void printResponse()
{
  if (confirmationResponseLoaded) {
    Serial.println("1");
  } else {
    Serial.println(reading);
  }
}

void setAlarmAndPowerDown()
{
  if (debug) {
    Serial.print("Setting alarm and powering down for ");
    Serial.print(alarmHours);
    Serial.print(":");
    Serial.print(alarmMinutes);
    Serial.print(":");
    Serial.println(alarmSeconds);
  }
  radio.flush_rx();
  radio.flush_tx();
  radio.powerDown();
  resetDatetime();
  setAlarm();
  delay(1000);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
}

void setAlarm()
{
  Clock.setA1Time(Day, alarmHours, alarmMinutes, alarmSeconds, 0x0, false, false, false);

  Clock.turnOnAlarm(1);
  Clock.turnOffAlarm(2);
  Clock.checkIfAlarm(1);
  Clock.checkIfAlarm(2);
}

void printDatetime()
{
  bool Century, h12 = false, PM = false;

  if (debug) {
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

void wakeUp()
{
  wokeUp = true;
  if (debug) {
    Serial.println("Woke up");
  }
}
