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
    return true;
  } else {
    // Received CONVERT Command
    return false;
  }
}

// Check if Boot can Proceed to SAFE
bool BootProceedCheck(id_t state)
{
  return BootCheck(state);
}

// Check if Boot Should Redirect to CONVERT
bool BootRedirectCheck(id_t state)
{
  return !BootCheck(state);
}

// Handle BOOT > SAFE
void BootSafeProcess(id_t state)
{
  SendRYLR("BOOTING FIRESIDE");

  // Setup Finish Pin and LED Status Indicator
  pinMode(STATUS_PIN, OUTPUT);
  digitalWrite(STATUS_PIN, HIGH);

  // Initialize Dedicated SPI Interface to SD Card
  // Abort if Initialization Fails
  if (!SD.begin())
  {
    ErrorBlink(ERR_SD_INIT);
    return;
  }

  SendRYLR("BOOT COMPLETE");
  SendRYLR("FIRESIDE SAFE");
}

// Handle BOOT > CONVERT
void BootConvertProcess(id_t state)
{
  SendRYLR("BOOT OVERRIDE");

  // Initialize Dedicated SPI Interface to SD Card
  // Abort if Initialization Fails
  if (!SD.begin())
  {
    ErrorBlink(ERR_SD_INIT);
    return;
  }

  // Find Last Logging File Name
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
  SendRYLR("BINARY FILENAME: " + FileName);

  // Double Check if Selected File Exists
  // Abort if File not Found
  if (!SD.exists(FileName))
  {
    ErrorBlink(ERR_SD_FILE);
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
    return true;
  } else {
    return false;
  }
}

// Handle SAFE > ARM
void SafeArmProcess(id_t state)
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
  if (!SD.open("TEST.ARM"), (O_CREAT | O_WRITE))
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
    return true;
  } else {
    // Return to SAFE State on any Other Command
    return false;
  }
}

// Check if ARM can Proceed to LAUNCH
bool ArmProceedCheck(id_t state)
{
  return ArmCheck(state);
}

// Check if ARM Should Redirect to SAFE
bool ArmRedirectCheck(id_t state)
{
  return !ArmCheck(state);
}

// Handle Arming Failure
void ArmFailureProcess(id_t state)
{
  SendRYLR("ARMING FAILURE");

  SendRYLR("ENSURING NO CURRENT TO IGNITERS");
  digitalWrite(FIRE_PIN_A, STATUS_SAFE);
  digitalWrite(FIRE_PIN_B, STATUS_SAFE);
  digitalWrite(FIRE_PIN_C, STATUS_SAFE);
}

// Handle Launch Command
void ArmLaunchProcess(id_t state)
{
  SendRYLR("FIRESIDE LAUNCH COMMAND");

  // Override Configuration Mode to Continuous
  bool ContinuousLogging = true;

  // Configure ADC for Data Acquisition
  ConfigureADC(ContinuousLogging);
  SendRYLR("ADC READY");

  // Configure DMA for Data Acquisition
  ConfigureDMA(ContinuousLogging);
  SendRYLR("DMA READY");

  // Configure Logging And Get Filename
  ConfigureLogging(FileName);
  SendRYLR("BINARY LOGGER READY");

  SendRYLR("FIRING IGNITERS");
}


// #### LAUNCH State Checks and Processes
// Check if LAUNCH can Proceed to LOGGING
bool LaunchCheck(id_t state)
{
  SendRYLR("LAUNCH & LOGGING TRIGGERED");

  // Pause FireSide RYLR Communications
  // There is not Enough CPU to Log and Communicate
  SendRYLR("RADIO SILENCE FIRESIDE");
  SendRYLR("SEND ANY COMMAND TO STOP LOGGING");

  // Any RYLR Input After This Point Interrupts Logging
  TriggerLogging();

  // Fire Igniters
  digitalWrite(FIRE_PIN_A, STATUS_FIRE);
  digitalWrite(FIRE_PIN_B, STATUS_FIRE);
  digitalWrite(FIRE_PIN_C, STATUS_FIRE);

  // Always Proceed to LOGGING
  return true;
}

// Handle Logging Indication After Firing Igniters
void LaunchLoggingProcess(id_t state)
{
  // Indicate Igniter Firing
  digitalWrite(STATUS_PIN, LOW);
}


// #### LOGGING State Checks and Processes
// Check if LOGGING is Complete
bool LoggingCheck(id_t state)
{
  // Write ADC Samples in DMA Buffers to Log File
  LogBuffers();

  // Check if Logging is Finished
  // Logging Stops If RYLR Receives Any Data
  if (RYLR.available())
  {
    // Signal Logging Finish to ADC and DMA Modules
    finished = true;
  }

  // Stay in LOGGING State Until LogFile is Closed
  if (LogFile.availableForWrite())
  {
    return false;
  } else {
    // Proceed to CONVERT State
    return true;
  }
}

// Handle Stop of Binary Logging
void LoggingConvertProcess(id_t state)
{
  SendRYLR("LOGGING STOPPED");

  // Open Log File to Output Diagnostics
  // Abort if SD Card IO Fails
  LogFile = SD.open(FileName, O_RDONLY);
  if (!LogFile)
  {
    ErrorBlink(ERR_SD_FILE);
    return;
  } else {
    SendRYLR("FILENAME: " + FileName);
    SendRYLR("FILESIZE: " + String(LogFile.size()));
    LogFile.close();
  }

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

  // Start Binary Log Conversion to CSV
  ConvertLog(FileName);

  // Always Proceed to SAFE State
  return true;
}

// Handle CONVERT > SAFE
void ConvertSafeProcess(id_t state)
{
  SendRYLR("BINARY CONVERSION COMPLETE");

  // Close Log File if Open
  if (LogFile)
  {
    LogFile.close();
  }

  // Reset Log File Name
  FileName = "";

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
  if (!SD.begin())
  {
    ErrorBlink(ERR_SD_INIT);
    return false;
  }

  // Ensure SD Card Functions
  // Abort on Failure
  SendRYLR("TESTING SDCARD");
  if (!SD.open("TEST.ARM"), (O_CREAT | O_WRITE))
  {
    ErrorBlink(ERR_SD_FILE);
    return false;
  }

  // Check Analog Inputs
  ReadoutAnalogPins();

  // Disable DMA
  HAL_ADC_Stop_DMA(&hadc1);
  SendRYLR("DMA DISABLED");

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
    return true;
  } else {
    // Rerun Diagnostics
    delay(500UL);
    return false;
  }
}

// Handle Safe State Transition After Failure
void FailureSafeProcess(id_t state)
{
  SendRYLR("SAFE COMMAND RECEIVED");
  SendRYLR("RESETTING TO SAFE");
}
