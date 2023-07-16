// (c) Walter Greger, internet.greger@me.com

//*****************************
// code for split flap master //
//*****************************

// libs
#include <Arduino.h>
#include <Wire.h> // for i2c communication
#include <WiFiEsp.h>
#include <WiFiEspUdp.h>
#include <SoftwareSerial.h> // to talk to esp8266 via uart

// Settings for connected units
const int displayUnits[] = {1, 2}; // array of i2c addresses; Index 0 will get first letter, index 1 the second,...
#define DISTRIBUTEUNITDELAY 500    // delay between units for letter transmission in ms.
#define BAUDRATE 115200            // baudrate for serial communication

// WIFI
SoftwareSerial Serial1(3, 2); // UART to ESP8266
#define ESPBAUDRATE 9600      // baudrate for ESP8266
int wifiStatus = WL_IDLE_STATUS;
const char ssid[] = "TheHorse";       // your network SSID (name)
const char pass[] = "1nternet";       // your network password, lmao pushing this to github what can go wrong
WiFiEspUDP Udp;                       // udp instance
const unsigned int localPort = 10002; // local port to listen on
const char ReplyBuffer[] = "ACK";     // a string to send back to UDP client after message received
char packetBuffer[255];               // buffer for received udp message

String currentMessage = "";

void setup()
{
  // setup Serial
  Serial.begin(BAUDRATE);
  Serial1.begin(ESPBAUDRATE);

  // initialize ESP module
  WiFi.init(&Serial1);
  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD)
  {
    Serial.println("WiFi shield not present");
    // don't continue
    while (true)
      ;
  }

  // attempt to connect to WiFi network
  while (wifiStatus != WL_CONNECTED)
  {
    Serial.print("attempting to connect to wpa SSID: ");
    Serial.println(ssid);
    wifiStatus = WiFi.begin(ssid, pass); // connect to wpa/wpa2 network
  }

  Serial.println("you're connected to the network!");
  IPAddress ip = WiFi.localIP(); // show IP adress
  Serial.print("IP-Address:");
  Serial.println(ip);

  Udp.begin(localPort); // start udp server for incoming messages
  Serial.print("listening on port ");
  Serial.println(localPort);

  Serial.println("starting i2c on master");
  Wire.begin(); // start i2c
}

void loop()
{
  // Input through serial
  if (Serial.available() > 0)
  {
    currentMessage = Serial.readString();
    Serial.println("(SERIAL) Received a message: " + currentMessage);

    distributeMessageToUnits(currentMessage);
  }

  // Input through UDP over WiFi
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    Serial.print("(UDP) Received a message: ");

    // Parse packet
    int len = Udp.read(packetBuffer, 255);
    if (len > 0)
    {
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

// this function sends message to units
void distributeMessageToUnits(String message)
{
  message.toUpperCase(); // convert characters to capital characters

  Serial.println("Showing message: " + message);

  unsigned int lastParseIndex = 0;
  int amountUnits = sizeof(displayUnits) / sizeof(int);
  byte bytes[message.length() + 1];
  message.getBytes(bytes, message.length() + 1);
  unsigned int amountBytes = sizeof(bytes) / sizeof(byte);

  // split message into utf letters with variable bytes
  for (int i = 0; i < amountUnits; i++)
  {
    if (lastParseIndex < amountBytes - 1)
    {
      // there is a letter
      for (unsigned int j = 0 + lastParseIndex; j < amountBytes - 1; j++)
      {
        int charLength = 1;
        if (bitRead(bytes[j], 7) && bitRead(bytes[j], 6))
        { // if true, utf char consists of at least two bytes, if false, letter consists of one byte
          charLength++;
          if (bitRead(bytes[j], 5))
            charLength++; // if true, utf char consists of at least three bytes
          if (bitRead(bytes[j], 4))
            charLength++; // if true, utf char consists of four bytes
        }

        byte aBuffer[charLength + 1]; // length corresponding to amount of bytes + one byte for NULL termination of string
        for (int k = 0; k < charLength; k++)
        {
          aBuffer[k] = bytes[k + j];
        }

        aBuffer[charLength] = 0; // terminate string with NULL byte at end

        // sending byte to i2c
        Wire.beginTransmission(displayUnits[i]);
        for (int k = 0; k < charLength + 1; k++)
        {
          Wire.write(aBuffer[k]);
        }
        Wire.endTransmission();

        // ask for unit for response (p, nil or o)
        getStatusOfUnit(displayUnits[i]);
        delay(DISTRIBUTEUNITDELAY);

        lastParseIndex = j + charLength;
        break;
      }
    }
    else
    {
      // no more letters for the unit, fill with spaces
      Wire.beginTransmission(displayUnits[i]);
      Wire.write((byte)32);
      Wire.write((byte)0);
      Wire.endTransmission();
      // ask for unit for response (p, nil or o)
      getStatusOfUnit(displayUnits[i]);
    }
  }
}

void getStatusOfUnit(int adress)
{
  // check for acknowledge and overheating of unit
  int answerBytes = Wire.requestFrom(adress, 1);
  int readIndex = 0;
  char unitAnswer[1];
  bool receivedAnswer = false;

  while (Wire.available())
  {
    unitAnswer[readIndex] = Wire.read();
    readIndex++;
    receivedAnswer = true;
  }

  // check answer
  if (unitAnswer[0] == 'P' && receivedAnswer)
    ; // received alive signal, nothing to do
  else if (unitAnswer[0] == 'O' && receivedAnswer)
    overheatingAlarm(adress); // an overheating was detected
  else if (receivedAnswer)
    receiveFailure(adress); // answer but weather p or o
  else if (answerBytes != 1)
    receiveFailure(adress); // no response
}

void overheatingAlarm(int adress)
{
  // an overheating was detected
  Serial.print("Unit ");
  Serial.print(adress);
  Serial.println(" sent an overheating alarm.");
  // put in some code to handle overheating, e.g. shutdown of display
}

void receiveFailure(int adress)
{
  // unit didn't answer correctly
  Serial.print("Unit ");
  Serial.print(adress);
  Serial.println(" didn't send a P.");
  // put in some code to handle missing ACK
}
