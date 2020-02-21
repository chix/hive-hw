#include <SPI.h>
#include <LoRa.h>

bool debug = true;
byte b;

void setup() {
  if (debug) {
    Serial.begin(9600);
    while (!Serial);
    Serial.println("LoRa Receiver");
  }

  LoRa.setPins(10, 9, 2);
  while (!LoRa.begin(433E6)) {
    if (debug) {
      Serial.println("Starting LoRa failed!");
    }
    delay(100);
  }
  if (debug) {
    Serial.println("Waiting for packets...");
  }
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    if (debug) {
      Serial.print("Received packet '");
    }

    // read packet
    while (LoRa.available()) {
      b = LoRa.read();
      if (debug) {
        Serial.print((char)b);
      }
    }

    if (debug) {
      Serial.print("' with RSSI ");
      Serial.println(LoRa.packetRssi());
    }
  }
}
