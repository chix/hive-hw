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

const bool debug = false;
const byte thisSlaveAddress[5] = {'H','0','0','0','1'}, confirmationResponse = 1;
int alarmHours = 1, alarmMinutes = 0, alarmSeconds = 0;
const float scale_calibration_factor = 20750.0, scale_zero_factor = -0.5;
char cmd, lastCmd;
float reading = 0;
unsigned long currentMillis = 0, prevMillis = 0, maxLoopMillis = 60000;
bool wokeUp = false, setupRun = true, confirmationResponseLoaded = true;

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

  // Reset alarms
  Clock.turnOffAlarm(1);
  Clock.turnOffAlarm(2);
  Clock.checkIfAlarm(1);
  Clock.checkIfAlarm(2);

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
    setupRun = false;
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
        // sync time with master during setup and afterwards each a week
        if (setupRun || getTimestamp() > 86400l * 7) {
          resetDatetime();
        }

        setAlarmAndPowerDown();
      }
      break;
  }

  currentMillis = millis();
  if (!setupRun && (currentMillis - prevMillis) >= maxLoopMillis) {
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
  delay(1000);
  radio.flush_rx();
  radio.flush_tx();
  radio.powerDown();
  setAlarm();
  delay(1000);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
}

void setAlarm()
{
  unsigned long timestamp = getTimestamp();
  unsigned long alarmTimestamp = 1l * alarmHours * 60 * 60 + alarmMinutes * 60 + alarmSeconds;
  unsigned long nextAlarmTimestamp = (timestamp / alarmTimestamp + 1) * alarmTimestamp;

  short int days = nextAlarmTimestamp / 86400l;
  short int hours = (nextAlarmTimestamp % 86400l) / 3600;
  short int minutes = ((nextAlarmTimestamp % 86400l) % 3600) / 60;
  short int seconds = ((nextAlarmTimestamp % 86400l) % 3600) % 60;

  if (debug) {
    Serial.print("Setting alarm and powering down until ");
    Serial.print(days);
    Serial.print("d ");
    Serial.print(hours);
    Serial.print(":");
    Serial.print(minutes);
    Serial.print(":");
    Serial.println(seconds);
  }

  Clock.setA1Time(days, hours, minutes, seconds, 0x0, false, false, false);

  Clock.turnOnAlarm(1);
  Clock.turnOffAlarm(2);
  Clock.checkIfAlarm(1);
  Clock.checkIfAlarm(2);
}

unsigned long getTimestamp()
{
  bool h12 = false, PM = false;
  unsigned long timestamp = 0;

  short int days = Clock.getDate();
  short int hours = Clock.getHour(h12, PM);
  short int minutes = Clock.getMinute();
  short int seconds = Clock.getSecond();
  timestamp = 1l * days * 24 * 60 * 60 + 1l * hours * 60 * 60 + minutes * 60 + seconds;

  return timestamp;
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
  Clock.setYear(0);
  Clock.setMonth(0);
  Clock.setDate(0);
  Clock.setHour(0);
  Clock.setMinute(0);
  Clock.setSecond(0);

  if (debug) {
    Serial.print("Resetting datetime to ");
    printDatetime();
  }
}

void wakeUp()
{
  wokeUp = true;
  if (debug) {
    Serial.println("Woke up");
  }
}
