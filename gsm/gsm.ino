#include <SoftwareSerial.h>

#define TX_PIN 7
#define RX_PIN 8
#define POWER_PIN 9

SoftwareSerial client(TX_PIN, RX_PIN); // SIM900 shield client

const String apiHost = "http://hive.martinkuric.cz";
String reading="{\"H1\":{\"w\":12345,\"t\":37.5},\"H2\":{\"w\":23456,\"t\":40.1}}";
String notification = "{\"H1\":true,\"H2\":true}";
const byte maxClientResponseSize = 32;
char clientResponseRow[maxClientResponseSize];
char clientResponse[maxClientResponseSize*6];
boolean newData = false;
boolean debug = true;

void setup()
{
  Serial.begin(9600);

  // Begin serial communication with Arduino and SIM900
  client.begin(9600);

  pinMode(POWER_PIN, OUTPUT);
  powerUpShield();
  //initGSM();
  //initGPRS();
  //sendData();
  //sendSetupNotification();
  powerDownShield();
}

void loop()
{
  delay(1000);
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

  client.println("AT");
  clientReadResponse();
  if (strstr(clientResponse, ".....") == NULL) {
    emulateShieldPowerButton();
  }

  i = 0;
  while (strstr(clientResponse, ".....") == NULL) {
    i++;
    if (i % 10 == 0) {
      emulateShieldPowerButton();
    }
    client.println("AT");
    clientReadResponse();
  }

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
  client.println("AT+HTTPINIT");
  clientReadResponse();
  client.println("AT+HTTPPARA=\"CID\",1");
  clientReadResponse();
  client.println("AT+HTTPPARA=\"URL\",\"" + apiHost + "/api/nodes/M1/data\"");
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
  client.println("AT+HTTPINIT");
  clientReadResponse();
  client.println("AT+HTTPPARA=\"CID\",1");
  clientReadResponse();
  client.println("AT+HTTPPARA=\"URL\",\"" + apiHost + "/api/nodes/M1/setup-notification\"");
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

  while (client.available() > 0 && newData == false) {
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
      newData = true;
    }
  }
}

void clientReadResponse()
{
  int i = 0;
  int j = 0;
  
  clientResponse[0] = '\0';
  for (j = 0; j <= 5; j++) {
    newData = false;
    i = 0;
    while (newData == false && i < 50) {
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
