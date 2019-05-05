#include <Servo.h>
Servo myservo;
int potpin = 0;
int val;

void setup() {
  buttonLockFunctionVariableInitialization();
}

void loop() {
  if (digitalRead(5) == 1) {
      turnLockClockwise(10);
  } else {
    stopLock();
  }
  delay(15); 
}

void buttonLockFunctionVariableInitialization(){
  pinMode(5, INPUT);
  myservo.attach(13);
  Serial.begin(9600);
}

void turnLockCounterClockwise(int time){
  for(int i = 0; i<time;i++){
    val = analogRead(potpin);
    val = map(val, 0, 1023, 180, 0);
    myservo.write(val);
  }
}

void turnLockClockwise(int time){
  for(int i = 0; i<time;i++){
    delay(200);
    val = analogRead(potpin);
    val = map(val, 0, 1023, 0, 180);
    myservo.write(val);
  }
}

void stopLock(){
  myservo.writeMicroseconds(1500);
}
