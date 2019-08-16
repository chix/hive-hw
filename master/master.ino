#include <DS3231.h>
#include <LowPower.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <Wire.h>

#define WAKE_UP_PIN 2
#define TX_PIN 7
#define RX_PIN 8
#define POWER_PIN 9
#define CE_PIN 6
#define CSN_PIN 10

SoftwareSerial client(TX_PIN, RX_PIN); // SIM900 shield client
DS3231 Clock;
RF24 radio(CE_PIN, CSN_PIN);

// TODO test debug false
const bool debug = true;
const String masterNodeCode = "M0001"; // should be a secure token
const byte numSlaves = 2;
const byte slaveAddress[numSlaves][5] = {
  {'H', '0', '0', '0', '1'},
  {'H', '0', '0', '0', '2'}
};
char cmdReading = 'R', cmdSleep = 'S', cmdExecute = 'X', cmdNothing = 'N';
byte slavesReadyForSleep[numSlaves] = {0, 0};
byte slavesSleeping[numSlaves] = {0, 0};
byte slaveConfirmation;
float slaveReading = 0, slaveReadings[numSlaves] = {0, 0};
unsigned long currentMillis = 0, prevMillis = 0, slaveSleepMillis = 0, maxLoopMillis = 56000; // set few seconds less than on slaves to prevent de-synchronization from radio errors
bool readingsDone, readyForSleep, sleeping = false;
int Year, Month, Day, Hour, Minute, Second = 0;
int alarmHours = 0, alarmMinutes = 0, alarmSeconds = 0; // set 1 minute less than on slave
const String apiHost = "http://hive.martinkuric.cz";
const byte maxClientResponseSize = 32;
char clientResponseRow[maxClientResponseSize];
char clientResponse[maxClientResponseSize*6];
bool newClientData = false, wokeUp = false;

void setup()
{
  if (debug) {
    Serial.begin(9600);
  }

  // Start the I2C interface
  Wire.begin();

  // Begin serial communication with Arduino and SIM900
  client.begin(9600);

  // Configure wake up pin
  pinMode(WAKE_UP_PIN, INPUT_PULLUP);
  digitalWrite(WAKE_UP_PIN, HIGH);
  attachInterrupt(digitalPinToInterrupt(WAKE_UP_PIN), wakeUp, FALLING);

  // Configure shield power pin
  pinMode(POWER_PIN, OUTPUT);

  // set up radio tx
  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.enableAckPayload();
  radio.setAutoAck(true);
  radio.enableDynamicPayloads();
  radio.stopListening();
  radio.setRetries(3, 5); // delay, count

  // setup:
  // wait until all slaves respond with readings (there's a 60s hard sleep timer on them)
  // then put them to sleep
  // then send setup notification
  // and go to sleep
  if (debug) {
    Serial.println("Setup");
  }
  getReadings(false);
  prepareForSleep(false);
  putToSleep(false);
  slaveSleepMillis = millis();
  powerUpShield();
  initGSM();
  initGPRS();
  sendSetupNotification();
  powerDownShield();
  currentMillis = millis();
  setAlarmAndPowerDown((currentMillis - slaveSleepMillis) / 1000);
}

void loop()
{
  // TODO reset alarms
  if (wokeUp) {
    prevMillis = millis();
    currentMillis = millis();
    readingsDone = false;
    readyForSleep = false;
    sleeping = false;
    radio.powerUp();
    radio.begin();
    radio.setDataRate(RF24_250KBPS);
    radio.enableAckPayload();
    radio.setAutoAck(true);
    radio.enableDynamicPayloads();
    radio.stopListening();
    radio.setRetries(3, 5); // delay, count
    wokeUp = false;
  }

  // get readings from slaves and put them to sleep
  getReadings(true);
  prepareForSleep(true);
  putToSleep(true);
  slaveSleepMillis = millis();

  // send data to server
  currentMillis = millis();
  if ((currentMillis - prevMillis) < maxLoopMillis) {
    powerUpShield();
    initGSM();
    initGPRS();
    sendData();
    powerDownShield();
  } else {
    // TODO maybe send a status report?
    if (debug) { Serial.println("Max loop time reached"); }
  }

  // go to sleep
  currentMillis = millis();
  setAlarmAndPowerDown((currentMillis - slaveSleepMillis) / 1000);
}

void getReadings(bool checkMaxLoopMillis)
{
  byte n;

  for (n = 0; n < numSlaves; n++) {
    slaveReadings[n] = 0;
  }
  while (readingsDone == false && (!checkMaxLoopMillis || (currentMillis - prevMillis) < maxLoopMillis)) {
    readingsDone = true;
    requestSlaveReadings();
    for (n = 0; n < numSlaves; n++) {
      if (slaveReadings[n] == 0) {
        readingsDone = false;
      }
    }
    delay(1000);
    currentMillis = millis();
  }
}

void prepareForSleep(bool checkMaxLoopMillis)
{
  byte n;
  
  for (n = 0; n < numSlaves; n++) {
    slavesReadyForSleep[n] = 0;
  }
  while (readyForSleep == false && (!checkMaxLoopMillis || (currentMillis - prevMillis) < maxLoopMillis)) {
    readyForSleep = true;
    prepareSlavesForSleep();
    for (n = 0; n < numSlaves; n++) {
      if (slavesReadyForSleep[n] == 0) {
        readyForSleep = false;
      }
    }
    delay(1000);
    currentMillis = millis();
  }
}

void putToSleep(bool checkMaxLoopMillis)
{
  byte n;
  
  for (n = 0; n < numSlaves; n++) {
    slavesSleeping[n] = 0;
  }
  while (sleeping == false && (!checkMaxLoopMillis || (currentMillis - prevMillis) < maxLoopMillis)) {
    sleeping = true;
    putSlavesToSleep();
    for (n = 0; n < numSlaves; n++) {
      if (slavesSleeping[n] == 0) {
        sleeping = false;
      }
    }
    delay(1000);
    currentMillis = millis();
  }
}

void requestSlaveReadings()
{
  bool rslt, erslt;
  byte i, n;
  
  for (n = 0; n < numSlaves; n++) {
    delay(100);
    if (slaveReadings[n] != 0) {
      continue;
    }

    radio.openWritingPipe(slaveAddress[n]);

    if (debug) {
      for (i = 0; i < 5; i++) { Serial.print(char(slaveAddress[n][i])); }
        Serial.print(" ");
        Serial.print(cmdReading);
        Serial.print(": ");
    }

    rslt = radio.write(&cmdReading, sizeof(cmdReading));

    if (rslt) {
      if (radio.isAckPayloadAvailable()) {
        radio.read(&slaveConfirmation, sizeof(slaveConfirmation));
        if (debug) {
          Serial.println(slaveConfirmation);
        }
      } else {
        slaveConfirmation = 0;
        if (debug) {
          Serial.println("no response");
        }
      }
      radio.openWritingPipe(slaveAddress[n]);
      if (debug) {
        for (i = 0; i < 5; i++) { Serial.print(char(slaveAddress[n][i])); }
        Serial.print(" ");
        Serial.print(cmdExecute);
        Serial.print(": ");
      }
      erslt = radio.write(&cmdExecute, sizeof(cmdExecute));
      if (erslt) {
        if (radio.isAckPayloadAvailable()) {
          radio.read(&slaveReading, sizeof(slaveReading));
          if (slaveConfirmation == 1) {
            slaveReadings[n] = slaveReading;
          }
          if (debug) {
            Serial.println(slaveReadings[n]);
          }
        } else {
          if (debug) { Serial.println("no response"); }
        }
      } else {
        if (debug) { Serial.println("error"); }
      }
    } else {
      if (debug) { Serial.println("error"); }
    }
  }
}

void prepareSlavesForSleep()
{
  bool rslt;
  byte n;
  
  for (n = 0; n < numSlaves; n++) {
    delay(100);
    if (slavesReadyForSleep[n] != 0) {
      continue;
    }

    radio.openWritingPipe(slaveAddress[n]);
    
    if (debug) {
      for (int i = 0; i < 5; i++) { Serial.print(char(slaveAddress[n][i])); }
        Serial.print(" ");
        Serial.print(cmdSleep);
        Serial.print(": ");
    }

    rslt = radio.write(&cmdSleep, sizeof(cmdSleep));

    if (rslt) {
      if (radio.isAckPayloadAvailable()) {
        radio.read(&slaveConfirmation, sizeof(slaveConfirmation));
        if (debug) {
          Serial.println(slaveConfirmation);
        }
      } else {
        slaveConfirmation = 0;
        if (debug) {
          Serial.println("no response");
        }        
      }
      if (slaveConfirmation == 1) {
        slavesReadyForSleep[n] = 1;
      }
    } else {
      if (debug) { Serial.println("error"); }
    }
  }
}

void putSlavesToSleep()
{
  bool rslt;
  byte i, n;
  
  for (n = 0; n < numSlaves; n++) {
    delay(100);
    if (slavesSleeping[n] != 0) {
      continue;
    }

    radio.openWritingPipe(slaveAddress[n]);
    
    if (debug) {
      for (i = 0; i < 5; i++) { Serial.print(char(slaveAddress[n][i])); }
        Serial.print(" ");
        Serial.print(cmdExecute);
        Serial.print(": ");
    }

    rslt = radio.write(&cmdExecute, sizeof(cmdExecute));

    if (rslt) {
      if (radio.isAckPayloadAvailable()) {
        radio.read(&slaveConfirmation, sizeof(slaveConfirmation));
        if (debug) {
          Serial.println(slaveConfirmation);
        }
      } else {
        slaveConfirmation = 0;
        if (debug) {
          Serial.println("no response");
        }        
      }
      if (slaveConfirmation == 1) {
        slavesSleeping[n] = 1;
      }
    } else {
      if (debug) { Serial.println("error"); }
    }
  }
}

void emulateShieldPowerButton()
{
  digitalWrite(9, HIGH);
  delay(1000);
  digitalWrite(9, LOW);
  delay(1000);
}

void powerUpShield()
{
  int i;
  
  if (debug) {
    Serial.println("Starting shield");
  }

  client.println("AT");
  clientReadResponse();
  if (strstr(clientResponse, ".....") != NULL) {
    emulateShieldPowerButton();
  }

  i = 0;
  while (strstr(clientResponse, "OK") == NULL) {
    i++;
    if (i % 10 == 0) {
      emulateShieldPowerButton();
    }
    client.println("AT");
    clientReadResponse();
  }
  
  if (debug) {
    Serial.println("Shield started");
  }
}

void powerDownShield()
{
  int i;
  
  if (debug) {
    Serial.println("Stopping shield");
  }

  client.println("AT+CPOWD=1");

  i = 0;
  do {
    i++;
    if (i % 10 == 0) {
      client.println("AT+CPOWD=0");
    }
    client.println("AT");
    clientReadResponse();
  } while (strstr(clientResponse, "......") == NULL);

  if (debug) {
    Serial.println("Shield stopped");
  }
}

void initGSM()
{
  client.println("AT");
  clientReadResponse();
  client.println("ATE1");
  clientReadResponse();
  client.println("AT+CPIN?"); // check SIM inserted
  clientReadResponse();
  client.println("AT+CCID"); // Read SIM information to confirm whether the SIM is plugged
  clientReadResponse();
  client.println("AT+CREG?"); // Check whether it has registered in the network
  clientReadResponse();
}

void initGPRS()
{
  client.println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  clientReadResponse();
  client.println("AT+SAPBR=3,1,\"APN\",\"TM\""); // APN, "TM" stands for ThinksMobile username
  clientReadResponse();
  client.println("AT+SAPBR=1,1");
  clientReadResponse();
  client.println("AT+SAPBR=2,1");
  clientReadResponse();
}

void sendData()
{
  byte n;
  String reading = "{\"";
  
  for (n = 0; n < numSlaves; n++) {
    reading.concat(char(slaveAddress[n][0]));
    reading.concat(char(slaveAddress[n][1]));
    reading.concat(char(slaveAddress[n][2]));
    reading.concat(char(slaveAddress[n][3]));
    reading.concat(char(slaveAddress[n][4]));
    reading.concat("\":{\"w\":");
    reading.concat(String(ceil(slaveReadings[n] * 1000)));
    reading.concat("}");
    if ((n + 1) < numSlaves) {
      reading.concat(",\"");
    }
  }
  reading.concat("}");

  if (debug) {
    Serial.println(reading);
    return;
  }

  client.println("AT+HTTPINIT");
  clientReadResponse();
  client.println("AT+HTTPPARA=\"CID\",1");
  clientReadResponse();
  client.println("AT+HTTPPARA=\"URL\",\"" + apiHost + "/api/nodes/" + masterNodeCode + "/data\"");
  clientReadResponse();
  client.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  clientReadResponse();
  client.println("AT+HTTPDATA=" + String(reading.length()) + ",100000");
  clientReadResponse();
  client.println(reading);
  clientReadResponse();
  client.println("AT+HTTPACTION=1");
  clientReadResponse();
  client.println("AT+HTTPREAD");
  clientReadResponse();
  client.println("AT+HTTPTERM");
  clientReadResponse();
}

void sendSetupNotification()
{
  byte n;
  String notification = "{\"";
  
  for (n = 0; n < numSlaves; n++) {
    notification.concat(char(slaveAddress[n][0]));
    notification.concat(char(slaveAddress[n][1]));
    notification.concat(char(slaveAddress[n][2]));
    notification.concat(char(slaveAddress[n][3]));
    notification.concat(char(slaveAddress[n][4]));
    notification.concat("\":{\"w\":");
    notification.concat(String(ceil(slaveReadings[n] * 1000)));
    notification.concat("}");
    if ((n + 1) < numSlaves) {
      notification.concat(",\"");
    }
  }
  notification.concat("}");

  if (debug) {
    Serial.println(notification);
    return;
  }

  client.println("AT+HTTPINIT");
  clientReadResponse();
  client.println("AT+HTTPPARA=\"CID\",1");
  clientReadResponse();
  client.println("AT+HTTPPARA=\"URL\",\"" + apiHost + "/api/nodes/" + masterNodeCode + "/setup-notification\"");
  clientReadResponse();
  client.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  clientReadResponse();
  client.println("AT+HTTPDATA=" + String(notification.length()) + ",100000");
  clientReadResponse();
  client.println(notification);
  clientReadResponse();
  client.println("AT+HTTPACTION=1");
  clientReadResponse();
  client.println("AT+HTTPREAD");
  clientReadResponse();
  client.println("AT+HTTPTERM");
  clientReadResponse();
}

void clientRead()
{
  static byte index = 0;
  char endMarker = '\n';
  char rc;

  while (client.available() > 0 && newClientData == false) {
    rc = client.read();

    if (rc != endMarker) {
      clientResponseRow[index] = rc;
      if (rc > 31 && rc < 127) {
        index++;
        if (index >= maxClientResponseSize) {
            index = maxClientResponseSize - 1;
        }
      }
    } else {
      clientResponseRow[index] = '\0';
      index = 0;
      newClientData = true;
    }
  }
}

void clientReadResponse()
{
  int i, j = 0;
  
  clientResponse[0] = '\0';
  for (j = 0; j <= 5; j++) {
    newClientData = false;
    i = 0;
    while (newClientData == false && i < 50) {
      i++;
      delay(10);
      clientRead();
    }
    if (i >= 50) {
      clientResponseRow[0] = '.';
      clientResponseRow[1] = '\0';
    }
    strcat(clientResponse, clientResponseRow);
  }
  if (debug) {
    Serial.println(clientResponse);
  }
}

void clientClear()
{
  while (client.available() > 0) {
    client.read();
  }
}

void setAlarmAndPowerDown(int secondsSinceSlaveSleep)
{
  int maxLoopSeconds = maxLoopMillis / 1000;
  
  alarmSeconds = maxLoopSeconds - secondsSinceSlaveSleep;

  if (alarmSeconds <= 0) {
    alarmSeconds = 1;
  }
  if (alarmSeconds > maxLoopSeconds) {
    alarmSeconds = maxLoopSeconds;
  }
  
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
  bool Century, h12, PM = false;

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
