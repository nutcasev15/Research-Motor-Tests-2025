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

float temp1Read, temp2Read;
int start;

typedef enum State {SAFE, ARMED, LAUNCHED, DONE, FAILURE};
State currentState = SAFE;

// Create an instance of the MAX6675 class with the specified pins
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
    // Find the next newline character after maxChunkSize
    endIndex = currState.indexOf('\n', startIndex + maxChunkSize);
    
    // If no newline found, use the end of the string
    if (endIndex == -1) {
      endIndex = currState.length();
    }

    // Extract the chunk to send
    String chunk = currState.substring(startIndex, endIndex);
    
    // Prepare and send the transmission
    String transmit = "AT+SEND=0," + String(chunk.length()) + "," + chunk + "\r\n";
    RYLR.print(transmit);
    
    // Wait for transmission to complete
    delay(10);

    // Move to the next chunk
    startIndex = endIndex + 1;
  }
}

void getData() 
{
  temp1Read = thermocouple1.readCelsius();
  temp2Read = thermocouple2.readCelsius();
  Serial.print("Temperature: ");
  Serial.print(temp1Read);
  Serial.print(" , ");
  Serial.println(temp2Read);
  String mess = String(temp1Read) + ";" + String(temp2Read);
  sendState(mess);
}

void checkInput(String receive) 
{
  receive.trim();
  receive = parseRYLR(receive);
  Serial.println(receive);
  if (receive == "ARM" && currentState == SAFE) 
  {
    digitalWrite(N_ENABLE, LOW);
    while(!(ucComm.available()||digitalRead(ACK)==LOW));
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
    start = millis();
  }
  // if (receive == "DONE" && currentState == LAUNCHED) 
  // {
  //   digitalWrite(N_ENABLE, HIGH);
  //   Serial.println("'DONE' received. Halting MKR Zero.");
  //   int delayTime = millis();
  //   while(digitalRead(ACK)==LOW || millis()-delayTime <= 5000);
  //   if(digitalRead(ACK)==HIGH)
  //   {
  //     Serial.println("DONE");
  //     currentState = DONE;
  //     sendState("TESTBED STATE: DONE");
  //     // delay(500);
  //   }
    // else
    // {
    //   Serial.println("MKRZero did not respond to DONE");  
    //   currentState = DONE;
    //   sendState("ERR=2; TESTBED STATE: DONE");
    // }
    
  //   return;
  // }
}

// void sendCollectedData() 
// {
//   Serial.println("Transmitting collected data...");
  
//   for (int row = 0; row <= currentRow; row++)
//   {
//     for (int i = 0; i < tempIndex[row]; i += 200) 
//     {  
//       String packet = String((char*)tempArray[row]).substring(i, i + 200);
//       ucComm.println(packet);
//       delay(50);
//     }
//   }

//   Serial.println("Data transmission complete!");
//   currentRow = 0;
//   for (int i = 0; i < maxRows; i++) 
//   {
//     tempIndex[i] = 0;
//     tempLogs[i] = 0;
//   }

//   RYLR.println("Approach Testbed");
// }

void performOperations() 
{
  switch (currentState) 
  {
    case SAFE:
      break;


    case ARMED:
      break;

    case LAUNCHED:
      // Serial.println("D4184s LATCHED");
      delay(20);
      
      digitalWrite(D4184A, HIGH);
      digitalWrite(D4184B, HIGH);

      getData();

      // if (ucComm.available()) {
      //   String backup = ucComm.readString();
      //   sendState(backup);
      // }
      
      // if(millis() - start < 20000) {
      //   digitalWrite(N_ENABLE, HIGH); 
      //   Serial.println("'DONE' received. Halting MKR Zero.");
      //   int delayTime = millis();
      //   while(digitalRead(ACK)==LOW || millis()-delayTime <= 5000);
      //   if(digitalRead(ACK)==HIGH)
      //   {
      //     Serial.println("DONE");
      //     currentState = DONE;
      //     sendState("TESTBED STATE: DONE");
      //     // delay(500);
      //   }
      // }
      break;

    // case DONE:
    //   digitalWrite(D4184A, LOW);
    //   digitalWrite(D4184B, LOW);
    //   while(1);  
    //   // sendCollectedData();
    //   break;

    case FAILURE:
      break;
  }
}

void setup() 
{
  Serial.begin(9600);
  RYLR.begin(57600);
  ucComm.begin(19200);
  //SPI.begin();
  
  pinMode(CS1, OUTPUT);
  pinMode(CS2, OUTPUT);
  pinMode(N_ENABLE, OUTPUT);
  pinMode(ACK, INPUT);

  digitalWrite(N_ENABLE, HIGH);

  pinMode(D4184A,OUTPUT);
  pinMode(D4184B,OUTPUT);

  // dataInit();
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