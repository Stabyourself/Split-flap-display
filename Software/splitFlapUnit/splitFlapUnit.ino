// based on code by Walter Greger

//***************************
// code for split flap unit //
//***************************

// libs
#include <Arduino.h>
#include <Wire.h>
#include <Stepper.h>
#include <Unistep2.h>

// constants i2c
#define i2cAddress 1 // i2c address

// constants stepper
#define STEPPERPIN1 8
#define STEPPERPIN2 9
#define STEPPERPIN3 10
#define STEPPERPIN4 11
#define STEPS 4096    // 28BYJ-48, number of steps;
int calOffset = 1115; // master sends these now
#define HALLPIN 5
#define STEPPERSPEED 900 // in rpm. 15 is about the maximum speed for the stepper to still be accurate

// constants others
#define BAUDRATE 115200
#define ROTATIONDIRECTION 1 //-1 for reverse direction

// globals
bool calibrationSuccess = false;
bool goingToMagnet = false;
int magnetAt = 0;
char displayedLetter = ' ';
char desiredLetter = ' ';
const char letters[] = {' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', ':', ',', '"', '!', '?', '-', '='};
Unistep2 stepper(STEPPERPIN1, STEPPERPIN2, STEPPERPIN3, STEPPERPIN4, STEPS, STEPPERSPEED);
byte byteBufferI2C[4]; // buffer for I2C read

// setup
void setup()
{
  // initialize serial
  Serial.begin(BAUDRATE);

  // initialize i2c
  Serial.println("starting i2c slave");
  Wire.begin(i2cAddress);      // i2c address of this unit
  Wire.onReceive(receiveData); // call-function if for transfered letter via i2c

  // setup motor
  pinMode(HALLPIN, INPUT);

  goToMagnet();
}

void loop()
{
  if (Serial.available() > 0)
  {
    String input = Serial.readString();
    desiredLetter = input.charAt(0);
  }

  // check if currently displayed letter differs from desired letter
  if (displayedLetter != desiredLetter && calibrationSuccess)
  {
    rotateToLetter(desiredLetter);
  }

  if (goingToMagnet)
  {
    if (isAtMagnet())
    {
      goingToMagnet = false;
      calibrationSuccess = true;
      stepper.stop();
      magnetAt = stepper.currentPosition();
      Serial.println("calibration success");

      rotateToLetter(desiredLetter);
    }
  }

  // set LED to hall sensor
  digitalWrite(LED_BUILTIN, (isAtMagnet()) ? HIGH : LOW);

  stepper.run();
}

bool isAtMagnet()
{
  return digitalRead(HALLPIN) == 0;
}

void goToMagnet()
{
  calibrationSuccess = false;
  goingToMagnet = true;
  stepper.move(STEPS);
}

// rotate to desired letter
void rotateToLetter(char toLetter)
{
  // get letter position
  int posLetter = -1;
  int posCurrentLetter = -1;
  int amountLetters = sizeof(letters) / sizeof(char);

  for (int i = 0; i < amountLetters; i++)
  {
    // current char
    if (toLetter == letters[i])
      posLetter = i;
  }

  // go to letter, but only if available (> -1)
  if (posLetter == -1)
  {
    desiredLetter = ' ';
  }
  else
  {
    int relativePosition = (stepper.currentPosition() - magnetAt + STEPS) % STEPS;

    // calculate at which step of the motor (relative to magnet) that letter is
    int letterAtStep = round((float)posLetter * ((float)STEPS / amountLetters) + calOffset);
    letterAtStep = letterAtStep % STEPS;

    int stepsToGo = letterAtStep - relativePosition;

    // if negative (ie a rollover), calibrate instead
    if (stepsToGo < 0)
    {
      Serial.println("passing magnet");
      goToMagnet();
    }
    else
    {
      Serial.print("go to letter:");
      Serial.println(toLetter);
      Serial.println("rotating to " + String(toLetter) + " at step " + String(letterAtStep) + " from " + String(relativePosition));

      if (stepsToGo > 0)
      {
        stepper.move(stepsToGo);
      }

      displayedLetter = toLetter;
    }
  }
}

void receiveData(int amount)
{
  if (amount == 0) // happens during i2c scanning
  {
    return;
  }

  Wire.readBytes(byteBufferI2C, amount);
  // Serial.println("received data");
  // Serial.println(byteBufferI2C[0]);
  // Serial.println(byteBufferI2C[1]);
  // Serial.println(byteBufferI2C[2]);
  // Serial.println(byteBufferI2C[3]);

  if (byteBufferI2C[0] == 255) // calOffset
  {
    // parse high and low byte into calOffset
    calOffset = byteBufferI2C[1] << 8 | byteBufferI2C[2];
    Serial.println("calOffset received: " + String(calOffset));
    goToMagnet();
    return;
  }
  else // everything else is just single byte letters
  {
    desiredLetter = (char)byteBufferI2C[0];
    Serial.println("letter received: " + String(desiredLetter));
    return;
  }
}
