// #### Library Headers
// Arduino Framework and Data Types
#include <Arduino.h>


// #### Internal Headers
// Hardware Interface Definitions and Functions
#include "Interfaces.hpp"

// DMA Data Logging Function Prototypes
#include "DMADAQ.hpp"

// Finite State Machine Definitions and Functions
#include "States.hpp"


// #### BOOT State Checks and Processes
// Check Boot State for RYLR Initialisation
bool BootCheck(id_t state)
{
  // Ensure Igniter MOSFETS are Off
  digitalWrite(FIRE_PIN_A, STATUS_SAFE);
  digitalWrite(FIRE_PIN_B, STATUS_SAFE);
  digitalWrite(FIRE_PIN_C, STATUS_SAFE);

  // Start RYLR Communication to GroundSide PCB
  RYLR.begin(RYLR_UART_BAUD);

  // Wait for GroundSide Contact
  while (!RYLR.available())
  {
    delay(500UL);
  }

  // Parse Command from GroundSide
  String command;
  ParseRYLR(command);

  // React to GroundSide State Command
  // Proceed to SAFE State
  if (command == "SAFE")
  {
    BootSafeTransition();
    return true;
  } else {
    // Received CONVERT Command
    BootConvertTransition();
    return false;
  }
}

// Handle BOOT > SAFE
void BootSafeTransition()
{
  SendRYLR("BOOTING FIRESIDE");

  // Setup Finish Pin and LED Status Indicator
  pinMode(STATUS_PIN, OUTPUT);
  digitalWrite(STATUS_PIN, HIGH);

  // Initialize Dedicated SPI Interface to SD Card
  // Abort if Initialization Fails
  if (!SD.begin(F_CPU / 4, SD_CHIP_SELECT_PIN))
  {
    ErrorBlink(ERR_SD_INIT);
    return;
  }

  SendRYLR("BOOT COMPLETE");
  SendRYLR("FIRESIDE SAFE");
}

// Handle BOOT > CONVERT
void BootConvertTransition()
{
  SendRYLR("BOOT OVERRIDE");

  // Initialize Dedicated SPI Interface to SD Card
  // Abort if Initialization Fails
  if (!SD.begin(F_CPU / 4, SD_CHIP_SELECT_PIN))
  {
    ErrorBlink(ERR_SD_INIT);
    return;
  }

  SendRYLR("OVERRIDE SUCCESSFUL");
}


// #### SAFE State Checks and Processes
// Check if System can Proceed to ARM
bool SafeCheck(id_t state)
{
  // Ensure Igniter MOSFETS are Off
  digitalWrite(FIRE_PIN_A, STATUS_SAFE);
  digitalWrite(FIRE_PIN_B, STATUS_SAFE);
  digitalWrite(FIRE_PIN_C, STATUS_SAFE);

  // Wait for GroundSide Command
  while (!RYLR.available())
  {
    delay(100UL);
  }

  // Parse Command from GroundSide
  String command;
  ParseRYLR(command);

  // Check if GroundSide Sent Correct Command
  if (command == "ARM")
  {
    SafeArmTransition();
    return true;
  } else {
    return false;
  }
}

// Handle SAFE > ARM
void SafeArmTransition()
{
  SendRYLR("ARMING FIRESIDE");

  // Check Analog Inputs
  ReadoutAnalogPins();

  SendRYLR("FIRESIDE ARMED");
}


// #### ARM State Checks and Processes
// Check if System can Proceed to LAUNCH
bool ArmCheck(id_t state)
{
  // Ensure Igniter MOSFETS are Off
  digitalWrite(FIRE_PIN_A, STATUS_SAFE);
  digitalWrite(FIRE_PIN_B, STATUS_SAFE);
  digitalWrite(FIRE_PIN_C, STATUS_SAFE);

  // Ensure SD Card Functions
  // Abort on Failure
  SendRYLR("TESTING SDCARD");
  if (!SD.open("Test.chk", FILE_WRITE))
  {
    ErrorBlink(ERR_SD_FILE);
    return false;
  }

  // Wait for Command from GroundSide
  while (!RYLR.available())
  {
    delay(100UL);
  }

  // Parse Command from GroundSide
  String command;
  ParseRYLR(command);

  // Check if GroundSide Sent Correct Command
  if (command == "LAUNCH")
  {
    ArmLaunchTransition();
    return true;
  } else {
    // Return to FAILURE State on any Other Command
    ArmFailureTransition();
    return false;
  }
}

// Handle Arming Failure
void ArmFailureTransition()
{
  SendRYLR("ARMING FAILURE");

  SendRYLR("ENSURING NO CURRENT TO IGNITERS");
  digitalWrite(FIRE_PIN_A, STATUS_SAFE);
  digitalWrite(FIRE_PIN_B, STATUS_SAFE);
  digitalWrite(FIRE_PIN_C, STATUS_SAFE);
}

// Handle Launch Command
void ArmLaunchTransition()
{
  SendRYLR("FIRESIDE LAUNCH COMMAND");

  // Override Configuration Mode to Continuous
  bool ContinuousLogging = true;

  // Configure DMA for Data Acquisition
  ConfigureDMA(ContinuousLogging);
  SendRYLR("DMA GO");

  // Configure ADC for Data Acquisition
  ConfigureADC(ContinuousLogging);
  SendRYLR("ADC GO");

  // Configure Logging And Get Filename
  ConfigureLogging();
  SendRYLR("BINARY LOGGER GO");
}


// #### LAUNCH State Checks and Processes
// Check if LAUNCH can Proceed to LOGGING
bool LaunchCheck(id_t state)
{
  // Pause FireSide RYLR Communications
  // There is not Enough CPU to Log and Communicate
  SendRYLR("RADIO SILENCE FIRESIDE");
  SendRYLR("SEND ANY COMMAND TO STOP LOGGING");
  SendRYLR("FIRING IGNITERS");

  // Any RYLR Input After This Point Interrupts Logging
  TriggerLogging();

  // Fire Igniters
  digitalWrite(FIRE_PIN_A, STATUS_FIRE);
  digitalWrite(FIRE_PIN_B, STATUS_FIRE);
  digitalWrite(FIRE_PIN_C, STATUS_FIRE);

  // Indicate Igniter Firing
  digitalWrite(STATUS_PIN, LOW);

  // Always Proceed to LOGGING
  return true;
}


// #### LOGGING State Checks and Processes
// Check if LOGGING is Complete
bool LoggingCheck(id_t state)
{
  // Write ADC Samples in DMA Buffers to Log File
  if (LogBuffers())
  {
    // Stay in LOGGING State Until LogFile is Closed
    return false;
  } else {
    // Proceed to CONVERT State
    LoggingConvertTransition();
    return true;
  }
}

// Handle Stop of Binary Logging
void LoggingConvertTransition()
{
  SendRYLR("LOGGING STOPPED");

  SendRYLR("CONVERTING BINARY LOG");
}


// #### CONVERT State Checks and Processes
// Check if CSV Conversion is Complete
bool ConvertCheck(id_t state)
{
  // Ensure Igniter MOSFETS are Off
  digitalWrite(FIRE_PIN_A, STATUS_SAFE);
  digitalWrite(FIRE_PIN_B, STATUS_SAFE);
  digitalWrite(FIRE_PIN_C, STATUS_SAFE);

  // Indicate Igniters are Safe
  digitalWrite(STATUS_PIN, HIGH);

  // Find Last Logging File Name
  // If a Log File Name is Already Assigned, Use That
  String FileName = GetLogfileName(false);

  // Otherwise, Search File System for Last Written Log File
  if (FileName.length() == 0)
  {
    for (short id = 0; (id + 1) >= 0; id++)
    {
      // Clear Existing File Name
      FileName = "";

      // Build and Test File Name
      FileName += (id + 1);
      FileName += ".dat";

      if (!SD.exists(FileName))
      {
        // Previous Tested ID was Last Log File
        // Clear File Name
        FileName = "";

        // Build File Name of Last Log File
        FileName += id;
        FileName += ".dat";

        // Select File and Stop Loop
        break;
      }
    }

    // Double Check if Selected File Exists
    // Abort if File not Found
    if (!SD.exists(FileName))
    {
      ErrorBlink(ERR_SD_FILE);
    }
  }

  // Start Binary Log Conversion to CSV
  SendRYLR("BINARY FILENAME: " + FileName);
  ConvertLog(FileName);

  // Always Proceed to SAFE State
  ConvertSafeTransition();
  return true;
}

// Handle CONVERT > SAFE
void ConvertSafeTransition()
{
  SendRYLR("BINARY CONVERSION COMPLETE");

  // Indicate Conversion is Complete
  digitalWrite(STATUS_PIN, LOW);

  SendRYLR("SAFING FIRESIDE");
}


// #### FAILURE State Checks and Processes
// Check why System is in a Failure State
bool FailureCheck(id_t state)
{
  SendRYLR("FIRESIDE FAILURE");

  // Turn Off Igniter MOSFETS
  SendRYLR("TURNING OFF IGNITERS");
  digitalWrite(FIRE_PIN_A, STATUS_SAFE);
  digitalWrite(FIRE_PIN_B, STATUS_SAFE);
  digitalWrite(FIRE_PIN_C, STATUS_SAFE);

  // Indicate Igniter Status
  digitalWrite(STATUS_PIN, HIGH);
  SendRYLR("IGNITERS OFF");

  // Check SD Card Status
  // Reinitialize Dedicated SPI Interface to SD Card
  // Abort if Initialization Fails
  SendRYLR("CHECKING SDCARD");
  SD.end();
  if (!SD.begin(F_CPU / 4, SD_CHIP_SELECT_PIN))
  {
    ErrorBlink(ERR_SD_INIT);
    return false;
  }

  // Ensure SD Card Functions
  // Abort on Failure
  SendRYLR("TESTING SDCARD");
  if (!SD.open("Test.chk", FILE_WRITE))
  {
    ErrorBlink(ERR_SD_FILE);
    return false;
  }

  // Check Analog Inputs
  ReadoutAnalogPins();

  // Wait for GroundSide Command
  while (!RYLR.available())
  {
    delay(500UL);
  }

  // Parse Command from GroundSide
  String command;
  ParseRYLR(command);

  // Check Received Command
  if (command == "SAFE")
  {
    // Only Proceed on Receipt of Safe Command
    SendRYLR("SAFE COMMAND RECEIVED");
    SendRYLR("RESETTING TO SAFE");
    return true;
  } else {
    // Rerun Diagnostics
    delay(5000UL);
    return false;
  }
}
