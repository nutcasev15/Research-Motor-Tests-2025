// #### Library Headers
// Arduino Framework and Data Types
#include <Arduino.h>

// Software Serial for RYLR Communication
#include <SoftwareSerial.h>


// #### Interface Definitions
// Command Switch Pins
#define ARM_SWITCH_PIN 2
#define LAUNCH_SWITCH_PIN 7


// RYLR998 Module Hardware Interface
// See REYAX RYLR998 Datasheet for UART Configuration
#define RYLR_UART_BAUD 115200UL
#define RYLR_UART_TX 3
#define RYLR_UART_RX 4

// Software Serial on Pins 3 & 4
SoftwareSerial RYLR(RYLR_UART_RX, RYLR_UART_TX);

// Parses Incoming Data from RYLR module
String ParseRYLR()
{
  if (!RYLR.available())
  {
    // Return Blank
    return String("\r\n");
  }

  // Read Incoming Data
  String parsed = RYLR.readString();

  // Extract Message Contents Following Last Comma
  parsed = parsed.substring(
    parsed.lastIndexOf(','),
    parsed.lastIndexOf('\r') - 1
  );

  // Remove Whitespace
  parsed.trim();

  return parsed;
}

// Sends State Commands via RYLR module
void SendRYLR(String state)
{
  // Check for Invalid Commands or Switches
  bool DefaultResponse = false;

  // Validate State Command
  if (!(state == "SAFE"\
    || state == "ARM"\
    || state == "LAUNCH"\
    || state == "CONVERT"))
  {
    Serial.println("INVALID COMMAND TO FIRESIDE");
    DefaultResponse = true;
  }

  // Check Switches for ARM State
  if (state == "ARM")
  {
    if (!(digitalRead(ARM_SWITCH_PIN) == HIGH \
      && digitalRead(LAUNCH_SWITCH_PIN) == LOW))
    {
      Serial.println("ARM SIGNAL MISMATCH");
      DefaultResponse = true;
    }
  }

  // Check Switches for LAUNCH State
  if (state == "LAUNCH")
  {
    if (!(digitalRead(ARM_SWITCH_PIN) == HIGH \
      && digitalRead(LAUNCH_SWITCH_PIN) == HIGH))
    {
      Serial.println("LAUNCH SIGNAL MISMATCH");
      DefaultResponse = true;
    }
  }

  if (DefaultResponse)
  {
    // Default to SAFE if Above Checks Fail
    Serial.println("SENDING SAFE COMMAND");
    state = String("SAFE");
  }

  // Send State Command to FireSide PCB
  RYLR.println(
    "AT+SEND=0," \
    + String(state.length()) + ',' \
    + state
  );
}


// #### Arduino UNO Hardware Setup
void setup() {
  // Initialize USB Serial Communication
  Serial.begin(RYLR_UART_BAUD);

  // Set Initial States for Switches
  digitalWrite(ARM_SWITCH_PIN, LOW);
  digitalWrite(LAUNCH_SWITCH_PIN, LOW);
  Serial.println("SYSTEM RESET");

  // Configure Switch Pins as Input
  pinMode(ARM_SWITCH_PIN, INPUT);
  pinMode(LAUNCH_SWITCH_PIN, INPUT);
  Serial.println("SYSTEM READY");

  // Establish Communication via RYLR module
  Serial.println("ESTABLISHING FIRESIDE LINK");
  RYLR.begin(RYLR_UART_BAUD);

  // Prompt User for FireSide PCB Initial State
  Serial.println("CHOOSE INITIAL STATE: SAFE || CONVERT");
  while (!Serial.available());
  // Send Initial State
  SendRYLR(Serial.readString());

  while (!RYLR.available());
  Serial.println("FIRESIDE LINK ACQUIRED");
}


// #### Arduino UNO Operations
void loop() {
  // Check for Incoming Data from FireSide PCB
  // Parse and Print Data to USB Serial
  if (RYLR.available())
  {
    Serial.println(ParseRYLR());
  }

  // Check for Incoming Commands from Serial Monitor
  // Send Received Command
  if (Serial.available())
  {
    SendRYLR(Serial.readString());
  }
}
