// based on code by Walter Greger

//***************************
// code for split flap unit //
//***************************

// libs
#include <Arduino.h>
#include <Wire.h>
#include <Stepper.h>

// constants i2c
#define i2cAddress 7 // i2c address

// constants stepper
#define STEPPERPIN1 8
#define STEPPERPIN2 9
#define STEPPERPIN3 10
#define STEPPERPIN4 11
#define STEPS 2038 // 28BYJ-48, number of steps;
int calOffset = 0; // master sends these now
#define HALLPIN 5
#define STEPPERSPEED 15 // in rpm. 15 is about the maximum speed for the stepper to still be accurate

// constants others
#define BAUDRATE 115200
#define ROTATIONDIRECTION 1 //-1 for reverse direction

// globals
bool newCalOffset = false;
bool calibrationSuccess = false;
char displayedLetter = ' ';
char desiredLetter = ' ';
int currentStep = 0;
const char letters[] = {' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', ':', ',', '"', '!', '?', '-', '='};
Stepper stepper(STEPS, STEPPERPIN1, STEPPERPIN3, STEPPERPIN2, STEPPERPIN4); // stepper setup
bool lastInd1 = false;                                                      // store last status of phase
bool lastInd2 = false;                                                      // store last status of phase
bool lastInd3 = false;                                                      // store last status of phase
bool lastInd4 = false;                                                      // store last status of phase
byte byteBufferI2C[4];                                                      // buffer for I2C read

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
  stepper.setSpeed(STEPPERSPEED);
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

  if (newCalOffset)
  {
    goToMagnet(); // going to zero point
    stepper.step(ROTATIONDIRECTION * calOffset);
    currentStep = calOffset;
    newCalOffset = false;
    stopMotor();
  }

  // set LED to hall sensor
  digitalWrite(LED_BUILTIN, (digitalRead(HALLPIN) == 0) ? HIGH : LOW);

  delay(100);
}

// doing a calibration of the revolver using the hall sensor
void goToMagnet()
{
  calibrationSuccess = false;
  bool reachedMarker = false;
  int i = 0;
  while (!reachedMarker)
  {
    int currentHallValue = digitalRead(HALLPIN);

    if (currentHallValue == 0 && i == 0)
    {
      // already in zero position move out a bit and do the calibration
      i = 100;
      stepper.step(ROTATIONDIRECTION * 100); // move 50 steps to get out of scope of hall
    }
    else if (currentHallValue == 1)
    {
      // not reached yet
      stepper.step(ROTATIONDIRECTION * 1);
    }
    else
    {
      Serial.println("found magnet");

      // reached marker
      reachedMarker = true;
      calibrationSuccess = true;
    }

    if (i > 3 * STEPS)
    {
      // seems that there is a problem with the marker or the sensor. turn of the motor to avoid overheating.
      Serial.println("magnet not found");
      reachedMarker = true;
    }

    i++;
  }
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

  Serial.print("go to letter:");
  Serial.println(toLetter);

  // go to letter, but only if available (> -1)
  if (posLetter > -1)
  {
    // calculate at which step of the motor (relative to magnet) that letter is
    int letterAtStep = round((float)posLetter * ((float)STEPS / amountLetters) + calOffset);
    if (letterAtStep > STEPS)
      letterAtStep -= STEPS;

    // calculate how many steps to go
    int stepsToGo = letterAtStep - currentStep;

    // if negative (ie a rollover), calibrate
    if (stepsToGo < 0)
    {
      Serial.println("passing magnet");
      goToMagnet();
      stepsToGo = letterAtStep;
    }

    // rotate
    stepper.step(ROTATIONDIRECTION * stepsToGo);

    displayedLetter = toLetter;
    currentStep = letterAtStep;

    // rotation is done, stop the motor
    stopMotor();
  }
}

// switching off the motor driver
void stopMotor()
{
  delay(100); // important to stop rotation before shutting of the motor to avoid rotation after switching off current

  lastInd1 = digitalRead(STEPPERPIN1);
  lastInd2 = digitalRead(STEPPERPIN2);
  lastInd3 = digitalRead(STEPPERPIN3);
  lastInd4 = digitalRead(STEPPERPIN4);

  digitalWrite(STEPPERPIN1, LOW);
  digitalWrite(STEPPERPIN2, LOW);
  digitalWrite(STEPPERPIN3, LOW);
  digitalWrite(STEPPERPIN4, LOW);
}

void startMotor()
{
  digitalWrite(STEPPERPIN1, lastInd1);
  digitalWrite(STEPPERPIN2, lastInd2);
  digitalWrite(STEPPERPIN3, lastInd3);
  digitalWrite(STEPPERPIN4, lastInd4);
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
    newCalOffset = true;
    return;
  }
  else // everything else is just single byte letters
  {
    desiredLetter = (char)byteBufferI2C[0];
    Serial.println("letter received: " + String(desiredLetter));
    return;
  }
}
