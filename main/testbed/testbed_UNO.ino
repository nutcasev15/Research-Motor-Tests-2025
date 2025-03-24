#include <max6675.h>
//#include <SPI.h>
#include <SoftwareSerial.h>
#include <AltSoftSerial.h>

#define SCK 13
#define SO 12
#define CS1 11
#define CS2 10

#define RYLR_RX 7
#define RYLR_TX 5
SoftwareSerial RYLR(RYLR_RX, RYLR_TX);
 
#define D4184A 3
#define D4184B 2

#define ACK A5
#define N_ENABLE A3

AltSoftSerial ucComm;

float temp1Buffer[4], temp2Buffer[4];
int tempIndex = 0;
unsigned long lastTempRead = 0;
const unsigned long tempInterval = 250;
unsigned long lastTransmitTime = 0;
const unsigned long transmitInterval = 1000;

typedef enum State {SAFE, ARMED, LAUNCHED, DONE, FAILURE};
State currentState = SAFE;

MAX6675 thermocouple1(SCK, CS1, SO);
MAX6675 thermocouple2(SCK, CS2, SO);

String parseRYLR(String input) {
  int start = input.indexOf(',') + 1;
  start = input.indexOf(',', start) + 1;
  int end = input.indexOf(',', start);
  String parsed = input.substring(start, end);
  parsed.trim();
  return parsed;  
}

void sendState(String currState) 
{
  const int maxChunkSize = 150;
  int startIndex = 0;
  int endIndex = 0;

  while (startIndex < currState.length()) {
    endIndex = currState.indexOf('\n', startIndex + maxChunkSize);
    if (endIndex == -1) {
      endIndex = currState.length();
    }

    String chunk = currState.substring(startIndex, endIndex);
    String transmit = "AT+SEND=0," + String(chunk.length()) + "," + chunk + "\r\n";
    RYLR.print(transmit);
    delay(10);

    startIndex = endIndex + 1;
  }
}

void getData() 
{
  temp1Buffer[tempIndex] = thermocouple1.readCelsius();
  temp2Buffer[tempIndex] = thermocouple2.readCelsius();
  tempIndex++;
  
  if (tempIndex >= 4) {
    tempIndex = 0;
    String mess = String(temp1Buffer[0]) + ";" + String(temp2Buffer[0]) + "|" +
                  String(temp1Buffer[1]) + ";" + String(temp2Buffer[1]) + "|" +
                  String(temp1Buffer[2]) + ";" + String(temp2Buffer[2]) + "|" +
                  String(temp1Buffer[3]) + ";" + String(temp2Buffer[3]);
    sendState(mess);
  }
}

void checkInput(String receive) 
{
  receive.trim();
  receive = parseRYLR(receive);
  Serial.println(receive);
  if (receive == "ARM" && currentState == SAFE) 
  {
    digitalWrite(N_ENABLE, LOW);
    while(digitalRead(ACK)==HIGH);
    if(digitalRead(ACK)==LOW)
    {
      Serial.println("CURRENT STATE: ARMED");
      currentState = ARMED;
      sendState("TESTBED STATE: ARMED");
      return;
    }
    else
    {
      Serial.println("MKRZero didnt respond to signal");  
      currentState = FAILURE;
      sendState("ERR=1; TESTBED STATE: FAILURE");
    }
    return;
  }
  if (receive == "DISARM" && currentState == ARMED) 
  {
    Serial.println("CURRENT STATE: SAFE");
    currentState = SAFE;
    sendState("TESTBED STATE: SAFE");
    return;
  }
  if (receive == "LAUNCH" && currentState == ARMED) 
  {
    Serial.println("CURRENT STATE: LAUNCHED");
    currentState = LAUNCHED;
    sendState("TESTBED STATE: LAUNCHED");
  }
  if (receive == "DONE" && currentState == LAUNCHED) 
  {
    digitalWrite(N_ENABLE, HIGH);
    Serial.println("'DONE' received. Halting MKR Zero.");

    digitalWrite(D4184A, LOW);
    digitalWrite(D4184B, LOW);

    int delayTime = millis();
    while(digitalRead(ACK)==LOW && millis()-delayTime <= 5000);
    if(digitalRead(ACK)==HIGH)
    {
      Serial.println("DONE");
      currentState = DONE;
      sendState("TESTBED STATE: DONE");
    }
    else
    {
      Serial.println("MKRZero did not respond to DONE");  
      currentState = DONE;
      sendState("ERR=2; TESTBED STATE: DONE");
    }
    return;
  }
}

void performOperations() 
{
  switch (currentState) 
  {
    case SAFE:
      break;

    case ARMED:
      break;

    case LAUNCHED:
      if (millis() - lastTempRead >= tempInterval) {
        lastTempRead = millis();
        getData();
      }
      digitalWrite(D4184A, HIGH);
      digitalWrite(D4184B, HIGH);
      
      if (RYLR.available()) {
        String input = RYLR.readStringUntil('\n');
        checkInput(input);
      }
      
      break;

    case DONE:
      delay(100);
      digitalWrite(D4184A, LOW);
      digitalWrite(D4184B, LOW);
      Serial.println("State: DONE - All outputs LOW");
      while(1);
      break;

    case FAILURE:
      break;
  }
}

void setup() 
{
  Serial.begin(9600);
  RYLR.begin(57600);
  ucComm.begin(19200);
  
  pinMode(CS1, OUTPUT);
  pinMode(CS2, OUTPUT);
  pinMode(N_ENABLE, OUTPUT);
  pinMode(ACK, INPUT);

  digitalWrite(N_ENABLE, HIGH);

  pinMode(D4184A,OUTPUT);
  pinMode(D4184B,OUTPUT);

  Serial.println("Setup Complete.");
}

void loop() 
{
  if (Serial.available()) 
  {
    String input = Serial.readStringUntil('\n');
    checkInput(input);
  }

  if (RYLR.available()) 
  {
    String input = RYLR.readStringUntil('\n');
    checkInput(input);
  }

  performOperations();
}
