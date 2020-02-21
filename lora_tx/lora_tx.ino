#include <SPI.h>
#include <LoRa.h>

int counter = 0;
bool debug = true;

void setup() {
  if (debug) {
    Serial.begin(9600);
    while (!Serial);
    Serial.println("LoRa Sender");
  }

  LoRa.setPins(10, 9, 2);
  if (!LoRa.begin(433E6)) {
    if (debug) {
      Serial.println("Starting LoRa failed!");
    }
    while(1);
  }

  LoRa.setTxPower(20);
}

void loop() {
  if (debug) {
    Serial.print("Sending packet: ");
    Serial.println(counter);
  }

  // send packet
  LoRa.beginPacket();
  LoRa.print("hello ");
  LoRa.print(counter);
  LoRa.endPacket();

  counter++;

  delay(1000);
}
