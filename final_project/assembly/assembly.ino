#include <SPI.h>
#include <Servo.h>
#include <MFRC522.h>
#include <DYE_Fingerprint.h>
#include <SoftwareSerial.h>

// PIN number
const int na0 = 0;
const int na1 = 1;
const int btnRegisterNewKnock = 2;
const int sensorHallMagnetic = 3;
const int sensorMicphone = 4;
const int sspinRFID = 5;
const int rstRFID = 6;
const int sensorFPInput = 7;
const int sensorFPOutput = 8;
const int na9 = 9;
const int servoMotor = 10;
const int RFID1 = 11;
const int RFID2 = 12;
const int RFID3 = 13;

// Constants
// For Knock
const int rejectThreshold = 25;         // Knock matching threshold
const int averageRejectThreshold = 15;  // Knock sequence matching threshold
const int knockMinimumGap = 150;      // Min knock gap
const int maximumKnocks = 20;       // Maximum number of knocks allow
const int knockTimeout = 1200;     // Know Waiting time

// Variable
int doorStatus;
int knockSensorValue, programButtonPressed, now;
// Initial Knock Storage
int storedKnock[maximumKnocks] = {50, 50, 50, 50, 50, 50, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int knockReadings[maximumKnocks];
// HallInterval
unsigned long hallPrevMillis = 0;
unsigned long hallHoldMillis = 2000; // 2s

// Binding Compoments
Servo doorServo;
MFRC522 mfrc522(sspinRFID, rstRFID);
SoftwareSerial fingerPrintSerial(sensorFPInput, sensorFPOutput);
DYE_Fingerprint finger = DYE_Fingerprint(&fingerPrintSerial);

void setup() {
  Serial.begin(9600);
  Serial.println("System: Data rates set to 9600, Start Initializing...");
  doorServo.attach(servoMotor);
  pinMode(btnRegisterNewKnock, INPUT);
  pinMode(sensorHallMagnetic, INPUT);
  pinMode(sensorMicphone, INPUT);
  Serial.println("System: Binding PINs...");


  // Initializing Finger Print Module
  while (!Serial);
  delay(100);
  Serial.println("Finger Print: Finding Module...");
  // Set the data rate for the sensor serial port
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("Finger Print: Found fingerprint sensor");
  } else {
    Serial.println("Finger Print: Did not find fingerprint sensor");
    while (1) {
      delay(1);
    }
  }

  doorStatus = 1;
  Serial.println("Door: Status set to 1 (open)");

  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("PFID Module: Card reader Initializing..");

  Serial.println("System: Initialized.");
}

void loop() {
  if (doorStatus == 1) {
    if (checkHall() == true) {
      closeDoor();
      return;
    }
  } else {
    // check knock
    knockSensorValue = digitalRead(sensorMicphone);
    if (digitalRead(btnRegisterNewKnock) == HIGH) {
      programButtonPressed = true;
    } else {
      programButtonPressed = false;
    }
    if (knockSensorValue == HIGH) {
      listenToKnock();
      return;
    }
    // check finger print
    if (getFingerprintIDez() >= 0) {
      openDoor();
      return;
    }
    // check RFID
    if (checkRFID() == true) {
      openDoor();
      return;
    }
  }



}

int openDoor() {
  doorServo.writeMicroseconds(1300);
  delay(245);
  doorServo.writeMicroseconds(1500);
  Serial.println("System: Door opened");
  doorStatus = 1;
  return 0;
}

int closeDoor() {
  doorServo.writeMicroseconds(1700);
  delay(255);
  doorServo.writeMicroseconds(1500);
  Serial.println("System: Door closed");
  doorStatus = 0;
  return 0;
}

boolean checkRFID() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return false;
  }
  //Show UID on serial monitor
  Serial.print("Reading a tag with id:");
  String content = "";
  byte letter;
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  content.toUpperCase();
  // Accessable Card ID, Change to add other access
  if (content.substring(1) == "E4 F6 E6 5F") {
    return true;
  } else {
    return false;
  }
}

void listenToKnock() {
  Serial.println("Knock Module: knock starting");

  int i = 0;
  for (i = 0; i < maximumKnocks; i++) {
    // Reset the listening array.
    knockReadings[i] = 0;
  }

  int currentKnockNumber = 0;
  int startTime = millis(); // First knock time
  delay(knockMinimumGap); // Wait before listen to the next knock.
  do {
    // Listen for the next knock
    knockSensorValue = digitalRead(sensorMicphone);
    if (knockSensorValue == 1) {
      Serial.println("Knock Module: knock");
      now = millis(); // Record the new knock time.
      knockReadings[currentKnockNumber] = now - startTime; // Add the gap to array
      currentKnockNumber++; //increment the counter
      startTime = now;
      delay(knockMinimumGap); // Wait before listen to the next knock.
    }
    now = millis();
  } while ((now - startTime < knockTimeout) && (currentKnockNumber < maximumKnocks));
  // Wait untill maximum kncok reached or timeout

  if (programButtonPressed == false) { // Knock listening mode
    if (validateKnock() == true) {
      closeDoor();
    } else {
      Serial.println("Knock Module: knock unmatch");
    }
  } else { // Knock reprogram mode
    validateKnock();
    // and we blink the green and red alternately to show that program is complete.
    Serial.println("Knock Module: new knock pattern stored");
  }
}

boolean validateKnock() {
  int i = 0;

  int validCheckingKnockCount = 0;
  int validTrueKnockCount = 0;
  int maxKnockInterval = 0;

  for (i = 0; i < maximumKnocks; i++) {
    if (knockReadings[i] > 0) {
      validCheckingKnockCount++;
    }
    if (storedKnock[i] > 0) {
      validTrueKnockCount++;
    }
    if (knockReadings[i] > maxKnockInterval) {
      maxKnockInterval = knockReadings[i];
    }
  }

  // Knock reprogram mode
  if (programButtonPressed == true) {
    for (i = 0; i < maximumKnocks; i++) { // Normalize the gap time
      storedKnock[i] = map(knockReadings[i], 0, maxKnockInterval, 0, 100);
    }
    return false; // Door donnot unlock
  }

  // Start checking pattern
  if (validCheckingKnockCount != validTrueKnockCount) {
    // First compare # knock
    return false;
  }

  // Check single far off
  int totaltimeDifferences = 0;
  int timeDiff = 0;
  for (i = 0; i < maximumKnocks; i++) { // Normalize the gap time
    knockReadings[i] = map(knockReadings[i], 0, maxKnockInterval, 0, 100);
    timeDiff = abs(knockReadings[i] - storedKnock[i]);
    if (timeDiff > rejectThreshold) { // Single knock value too far off
      return false;
    }
    totaltimeDifferences += timeDiff;
  }
  // Check average far off
  if (totaltimeDifferences / validTrueKnockCount > averageRejectThreshold) {
    return false;
  }
  return true;
}

int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    return -1;
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    return -1;
  }

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) {
    return -1;
  }

  Serial.print("Finger Print: Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);
  return finger.fingerID;
}

boolean checkHall() {
  int hallStatus = digitalRead(sensorHallMagnetic);
  if (hallStatus == HIGH) {
    hallPrevMillis = millis();
    while (digitalRead(sensorHallMagnetic) == HIGH) {
      if (millis() - hallPrevMillis > hallHoldMillis) {
        Serial.println("Hall Magnetic: Door closed for 2s");
        return true;
      }
    }
  }
  return false;
}
