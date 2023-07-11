// (c) Walter Greger, internet.greger@me.com

//*****************************
// code for split flap master //
//*****************************

// libs
#include <Arduino.h>
#include <Wire.h> //for i2c communication

// Settings for connected units
const int displayUnits[] = {1, 2}; // array of i2c addresses; Index 0 will get first letter, index 1 the second,...
#define DISTRIBUTEUNITDELAY 0      // delay between units for letter transmission in ms.
#define BAUDRATE 115200            // baudrate for serial communication

// Settings for UDP server for message transmission
String currentMessage = "";

void setup()
{
  // setup Serial
  Serial.begin(BAUDRATE);

  Serial.println("starting i2c on master");
  Wire.begin(); // start i2c
}

void loop()
{
  if (Serial.available() > 0)
  {
    String currentMessage = Serial.readString();
    distributeMessageToUnits(currentMessage);
  }
}

// this function sends message to units
void distributeMessageToUnits(String message)
{
  Serial.println("Showing message: " + message);

  message.toUpperCase(); // convert characters to capital characters

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
  Serial.println(" didn't send an P.");
  // put in some code to handle missing ACK
}
