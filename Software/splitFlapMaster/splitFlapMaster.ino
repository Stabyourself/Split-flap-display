// based on code by Walter Greger

//*****************************
// code for split flap master //
//*****************************

// libs
#include <Arduino.h>
#include <SoftwareSerial.h>  // to talk to esp8266 via uart
#include <WiFiEsp.h>
#include <WiFiEspUdp.h>
#include <Wire.h>  // for i2c communication

#define BAUDRATE 115200  // baudrate for serial communication

// Settings for connected units
const int potentialUnits[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};  // array of i2c addresses
const int potentialCalOffsets[] = {1130, 1100, 1240, 1145, 1195,
                                   1060, 1060, 1313, 1165, 1232,
                                   1225, 1260, 1220, 1230, 1180,
                                   1230, 1300, 1200, 1260, 0};  // array of calibration offsets for units, higher values move "down"
int units[sizeof(potentialUnits) / sizeof(int)];                // array of found units
int calOffsets[sizeof(potentialCalOffsets) / sizeof(int)];      // array of found calibration offsets
int amountUnits;                                                // amount of found units
#define DISTRIBUTEUNITDELAY 0                                   // delay between units for letter transmission in ms.

// WIFI
#define WIFIENABLED false              // set to false if you don't want to use wifi
SoftwareSerial SerialESP(3, 2);        // UART to ESP8266
#define ESPBAUDRATE 9600               // baudrate for ESP8266
int wifiStatus = WL_IDLE_STATUS;       // wifi status
const char ssid[] = "TheHorse";        // your network SSID (name)
const char pass[] = "1nternet";        // your network password, lmao pushing this to github what can go wrong
WiFiEspUDP Udp;                        // udp instance
const unsigned int localPort = 10002;  // local port to listen on
const char ReplyBuffer[] = "ACK";      // a string to send back to UDP client after message received
char packetBuffer[255];                // buffer for received udp message

String currentMessage = "";

void setup() {
  // setup Serial
  Serial.begin(BAUDRATE);

  if (WIFIENABLED) {
    SerialESP.begin(ESPBAUDRATE);

    // initialize ESP module
    WiFi.init(&SerialESP);
    // check for the presence of the shield
    if (WiFi.status() == WL_NO_SHIELD) {
      Serial.println("WiFi shield not present");
      // don't continue
      while (true);
    }

    // attempt to connect to WiFi network
    while (wifiStatus != WL_CONNECTED) {
      Serial.print("attempting to connect to wpa SSID: ");
      Serial.println(ssid);
      wifiStatus = WiFi.begin(ssid, pass);  // connect to wpa/wpa2 network
    }

    Serial.println("you're connected to the network!");
    IPAddress ip = WiFi.localIP();  // show IP adress
    Serial.print("IP-Address:");
    Serial.println(ip);

    Udp.begin(localPort);  // start udp server for incoming messages
    Serial.print("listening on port ");
    Serial.println(localPort);
  }

  Serial.println("starting i2c on master");
  Wire.begin();  // start i2c

  delay(1000);  // wait a bit for units

  scanForUnits();

  Serial.println("Sending caloffsets");
  sendCalOffsets();

  Serial.println("Ready!");
}

void scanForUnits() {
  Serial.println("scanning for units...");
  // scan for units
  int amountPotentialUnits = sizeof(potentialUnits) / sizeof(int);
  amountUnits = 0;
  for (int i = 0; i < amountPotentialUnits; i++) {
    Wire.beginTransmission(potentialUnits[i]);
    byte error = Wire.endTransmission();
    if (error == 0) {
      units[amountUnits] = potentialUnits[i];
      calOffsets[amountUnits] = potentialCalOffsets[i];
      amountUnits++;
    }
  }

  // print found units
  Serial.println("found " + String(amountUnits) + " units:");
  for (int i = 0; i < amountUnits; i++) {
    Serial.print(String(units[i]) + " ");
  }
  Serial.println("");
}

void sendCalOffsets() {
  // send calOffsets
  for (int i = 0; i < amountUnits; i++) {
    Wire.beginTransmission(units[i]);
    Wire.write((byte)255);  // tells the unit that this is a calOffset
    Wire.write(highByte(calOffsets[i]));
    Wire.write(lowByte(calOffsets[i]));
    Wire.endTransmission();
  }
}

void loop() {
  // Input through serial
  if (Serial.available() > 0) {
    currentMessage = Serial.readString();
    Serial.println("(SERIAL) Received a message: " + currentMessage);

    distributeMessageToUnits(currentMessage);
  }

  if (WIFIENABLED) {
    // Input through UDP over WiFi
    int packetSize = Udp.parsePacket();
    if (packetSize) {
      Serial.print("(UDP) Received a message: ");

      // Parse packet
      int len = Udp.read(packetBuffer, 255);
      if (len > 0) {
        packetBuffer[len] = 0;
      }

      // convert bytes to string
      currentMessage = String((char *)packetBuffer);
      Serial.println(currentMessage);

      // send a reply, to the IP address and port that sent us the packet we received
      Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
      Udp.write(ReplyBuffer);
      Udp.endPacket();

      distributeMessageToUnits(currentMessage);
    }
  }
}

// this function sends message to units
void distributeMessageToUnits(String message) {
  message.toUpperCase();  // convert characters to capital characters

  Serial.println("Showing message: " + message);

  for (int i = 0; i < amountUnits; i++) {
    char letterToSend = ' ';

    if (i < message.length()) {
      letterToSend = message.charAt(i);
    }

    // sending byte to i2c
    Wire.beginTransmission(units[i]);
    Wire.write(letterToSend);
    Wire.endTransmission();

    delay(DISTRIBUTEUNITDELAY);
  }
}
