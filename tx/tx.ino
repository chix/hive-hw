#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define CE_PIN 9
#define CSN_PIN 10

RF24 radio(CE_PIN, CSN_PIN);

const byte numSlaves = 2;
const byte slaveAddress[numSlaves][5] = {
  {'R', 'x', 'A', 'A', 'A'},
  {'R', 'x', 'A', 'A', 'B'}
};
char dataToSend[10] = "ToSlvN  0";
int ackData[2] = { -1, -1};
char messageNumber = '0';
bool newData = false;
unsigned long currentMillis;
unsigned long prevMillis;
unsigned long txIntervalMillis = 1000;

void setup()
{
  Serial.begin(9600);
  Serial.println("SimpleTxAckPayload Starting");

  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.enableAckPayload();
  radio.setAutoAck(true);
  radio.enableDynamicPayloads();
  radio.stopListening();

  radio.setRetries(3, 5); // delay, count
}

void loop()
{
  currentMillis = millis();
  if (currentMillis - prevMillis >= txIntervalMillis) {
    send();
  }
}

void send()
{
  for (byte n = 0; n < numSlaves; n++) {

    // open the writing pipe with the address of a slave
    radio.openWritingPipe(slaveAddress[n]);

    dataToSend[5] = n + '0';

    bool rslt;
    rslt = radio.write(&dataToSend, sizeof(dataToSend));

    Serial.print("  ========  For Slave ");
    Serial.print(n);
    Serial.println("  ========");
    Serial.print("  Data Sent ");
    Serial.print(dataToSend);
    if (rslt) {
      if (radio.isAckPayloadAvailable()) {
        radio.read(&ackData, sizeof(ackData));
        newData = true;
      } else {
        Serial.println("  Acknowledge but no data ");
      }
      updateMessage();
    } else {
      Serial.println("  Tx failed");
    }
    showData();
    Serial.print("\n");
  }

  prevMillis = millis();
}

void showData()
{
  if (newData == true) {
    Serial.print("  Acknowledge data ");
    Serial.print(ackData[0]);
    Serial.print(", ");
    Serial.println(ackData[1]);
    Serial.println();
    newData = false;
  }
}

void updateMessage()
{
  messageNumber += 1;
  if (messageNumber > '9') {
    messageNumber = '0';
  }
  dataToSend[8] = messageNumber;
}
