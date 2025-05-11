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

// Parses Incoming Data from FireSide PCB via RYLR module
void ParseRYLR(String &Buffer)
{
  if (!RYLR.available())
  {
    // Return Blank
    Buffer = "\r\n";
    return;
  }

  // Load Incoming Data
  String parsed = RYLR.readString();


  // See +RCV in REYAX AT RYLRX98 Commanding Datasheet
  // Remove Data from Last 2 Fields
  parsed.remove(parsed.lastIndexOf(','));
  parsed.remove(parsed.lastIndexOf(','));

  // Extract Data in 3rd Comma Separated Field
  Buffer = parsed.substring(parsed.lastIndexOf(',') + 1);

  // Remove Whitespace
  Buffer.trim();

  return;
}

// Incoming Response Buffer for RYLR
String FireSideResponse;

// Sends State Commands to FireSide PCB via RYLR module
void SendRYLR(const String &State)
{
  // Check for Invalid Commands or Switches
  bool OverrideResponse = false;

  // Validate State Command
  if (!(State == "SAFE"\
    || State == "ARM"\
    || State == "LAUNCH"\
    || State == "CONVERT"))
  {
    Serial.println("INVALID COMMAND TO FIRESIDE");
    OverrideResponse = true;
  }

  // Check Switches for ARM State
  if (State == "ARM")
  {
    if (!(digitalRead(ARM_SWITCH_PIN) == HIGH \
      && digitalRead(LAUNCH_SWITCH_PIN) == LOW))
    {
      Serial.println("ARM SIGNAL MISMATCH");
      OverrideResponse = true;
    }
  }

  // Check Switches for LAUNCH State
  if (State == "LAUNCH")
  {
    if (!(digitalRead(ARM_SWITCH_PIN) == HIGH \
      && digitalRead(LAUNCH_SWITCH_PIN) == HIGH))
    {
      Serial.println("LAUNCH SIGNAL MISMATCH");
      OverrideResponse = true;
    }
  }

  // Issue Send AT Command
  // See +SEND in REYAX AT RYLRX98 Commanding Datasheet
  RYLR.print("AT+SEND=0,");

  // Default to SAFE if Above Checks Fail
  if (OverrideResponse)
  {
    Serial.println("SENDING SAFE COMMAND");

    // Issue Payload Length
    // 4 Characters for SAFE Command
    RYLR.print(4);

    // Add Comma Separator
    RYLR.print(',');

    // Complete Command with Line End
    RYLR.println("SAFE");
  } else {
    // Issue Payload Length
    RYLR.print(State.length());

    // Add Comma Separator
    RYLR.print(',');

    // Complete Command with Line End
    RYLR.println(State);
  }
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

  // Reserve Memory for Incoming RYLR Data
  FireSideResponse.reserve(256UL);

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
    // Clear Response Buffer and Load Data
    FireSideResponse = "";
    ParseRYLR(FireSideResponse);

    Serial.println(FireSideResponse);
  }

  // Check for Incoming Commands from Serial Monitor
  // Send Received Command
  if (Serial.available())
  {
    SendRYLR(Serial.readString());
  }
}
