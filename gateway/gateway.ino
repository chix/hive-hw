/**
 esp + uno proxy circuit https://hacksterio.s3.amazonaws.com/uploads/attachments/338103/uno_esp_connection_hO79gxjiVX.png 
 upload a blank script (or ground reset pin), set boud rate to 115200, use AT/Backlog commands to communicate

 esp + ftdi converter flashing circuit https://cdn.instructables.com/F3C/NX6E/JZ0N1XXM/F3CNX6EJZ0N1XXM.LARGE.jpg
 use esptool.py, flash size is 1MB
 info:    esptool.py flash_id
 backup:  esptool.py --port /dev/ttyUSB4 read_flash 0x00000 0x100000 backup.bin
 recover: esptool.py --port /dev/ttyUSB4 write_flash -fs 1MB 0x0 backup.bin
 clear:   esptool.py --port /dev/ttyUSB4 erase_flash
 upload:  esptool.py --port /dev/ttyUSB4 write_flash -fs 1MB -fm dout 0x0 sonoff.bin
 https://tasmota.github.io/docs - web UI configuration to wi-fi auto-connect
*/

#include <LoRa.h>
#include <rBase64.h>
#include <SoftwareSerial.h>
#include <SPI.h>

#define LORA_GPI 2
#define LORA_RST 9
#define LORA_NSS 10

bool debug = false;
const String masterNodeCode = "M0001"; // should be a secure token
const String apiHost = "hive.martinkuric.cz";
String command, message, path;

void setup() {
  Serial.begin(115200);

  // Configure lora pins
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_GPI);

  while (!LoRa.begin(433E6)) {
    if (debug) {
      Serial.println("Starting LoRa failed!");
    }
    delay(1000);
  }
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(62.5E3);
  LoRa.setCodingRate4(8);

  if (debug) {
    Serial.println("Gateway started.");
  }
}

void loop() {
  int packetSize = LoRa.parsePacket();

  if (packetSize) {
    // read message
    message = "";
    path = "";
    while (LoRa.available()) {
      message.concat((char)LoRa.read());
    }

    if (debug) {
      Serial.print("Received message '");
      Serial.print(message);
      Serial.print("' with RSSI ");
      Serial.println(LoRa.packetRssi());
    }

    // first char of message specifies the endpoint
    if (message.charAt(0) == 'D') {
      path = "/data/";
    }else if (message.charAt(0) == 'S') {
      path = "/setup-notification/";
    } else if (message.charAt(0) == 'E') {
      path = "/error-report/";
    }

    // compose WebSend command
    command = "WebSend [";
    command.concat(apiHost);
    command.concat("] /api/nodes/");
    command.concat(masterNodeCode);
    command.concat(path);
    rbase64.encode(message.substring(1)); // the rest of message is json
    command.concat(rbase64.result()); // which is base64 encoded, because it has to be sent via GET

    // send command to ESP
    Serial.println(command);
  }
}
