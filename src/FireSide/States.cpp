// #### Library Headers
// Arduino Framework and Data Types
#include <Arduino.h>

// SD Card SPI Initialisation Clock Rates
#include <SPI.h>


// #### Internal Headers
// Hardware Interface Definitions and Functions
#include "Interfaces.hpp"

// DMA Data Logging Function Prototypes
#include "DMADAQ.hpp"

// Finite State Machine Definitions and Functions
#include "States.hpp"


// #### BOOT State Checks and Processes
// Check The Boot State for RYLR Initialisation
bool BootCheck(id_t state)
{
  // Ensure Igniter MOSFETS are Off
  digitalWrite(FIRE_PIN_A, LOW);
  digitalWrite(FIRE_PIN_B, LOW);

  // Start RYLR Communication to GroundSide PCB
  RYLR.begin(RYLR_UART_BAUD);

  // Wait for GroundSide Contact
  while (!RYLR.available())
  {
    delay(500UL);
  }

  // React to GroundSide State Command
  // Proceed to SAFE State
  if (ParseRYLR() == "SAFE")
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
  // Use Maximum SPI SCK Frequency Provided in SAMD21 Datasheet
  if (!SD.begin(F_CPU / SPI_MIN_CLOCK_DIVIDER, SDCARD_SS_PIN))
  {
    ErrorBlink(ERR_SD_INIT);
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
  // Use Maximum SPI SCK Frequency Provided in SAMD21 Datasheet
  if (!SD.begin(F_CPU / SPI_MIN_CLOCK_DIVIDER, SDCARD_SS_PIN))
  {
    ErrorBlink(ERR_SD_INIT);
  }

  // Find Last Logging File Name
  for (short id = -1; (id + 1) >= 0; id++)
  {
    if (!SD.exists(String(id + 1) + ".dat"))
    {
      FileName = String(id) + ".dat";
      break;
    }
  }
  SendRYLR("BINARY FILENAME: " + FileName);

  // Double Check if the Selected File Exists
  // Abort if File not Found
  if (!SD.exists(FileName))
  {
    ErrorBlink(ERR_SD_FILE);
  }

  SendRYLR("OVERRIDE SUCCESSFUL");
}


// #### SAFE State Checks and Processes
// Check if the System can Proceed to ARM
bool SafeCheck(id_t state)
{
  // Ensure Igniter MOSFETS are Off
  digitalWrite(FIRE_PIN_A, LOW);
  digitalWrite(FIRE_PIN_B, LOW);

  // Wait for GroundSide Command
  while (!RYLR.available())
  {
    delay(100UL);
  }

  // Check if GroundSide Sent Correct Command
  if (ParseRYLR() == "ARM")
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

  // Assemble ADC Channel Debug Data
  String debug;
  for(short channel = A1; channel <= A6; channel++)
  {
    debug += " A" \
      + String(channel - A1 + 1) \
      + '=' \
      + String(analogRead(channel) * 3.3 / 1024.0, 3) \
      + 'V';
  }

  // Transmit ADC Channel Debug Data over RYLR
  SendRYLR("ADC CHANNEL STATUS");
  SendRYLR(debug);

  // Configure DMA For Data Acquisition
  ConfigureDMA();
  SendRYLR("DMA READY");

  SendRYLR("FIRESIDE ARMED");
}


// #### ARM State Checks and Processes
// Check if the System can Proceed to LAUNCH
bool ArmCheck(id_t state)
{
  // Ensure Igniter MOSFETS are Off
  digitalWrite(FIRE_PIN_A, LOW);
  digitalWrite(FIRE_PIN_B, LOW);

  // Ensure SD Card Functions
  SendRYLR("TESTING SDCARD");
  if (!SD.open("TEST.ARM"), (O_CREAT | O_WRITE))
  {
    SendRYLR("SDCARD TEST FAILED");
  }

  // Wait for Command from GroundSide
  while (!RYLR.available())
  {
    delay(100UL);
  }

  // Check if GroundSide Sent Correct Command
  if (ParseRYLR() == "LAUNCH")
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
  digitalWrite(FIRE_PIN_A, LOW);
  digitalWrite(FIRE_PIN_B, LOW);
}

// Handle Launch Command
void ArmLaunchProcess(id_t state)
{
  SendRYLR("FIRESIDE LAUNCH COMMAND");

  // Configure ADC for Data Acquisition
  ConfigureADC();
  SendRYLR("ADC READY");

  // Configure Logging And Get Filename
  FileName = ConfigureLogging();
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

  // Fire the Igniters
  digitalWrite(FIRE_PIN_A, HIGH);
  digitalWrite(FIRE_PIN_B, HIGH);

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
  // Stop Signal Checked in DMA Transfer Handler
  // See HandleTransferComplete() in DMADAQ.cpp
  if (finished)
  {
    // Proceed to CONVERT State
    return true;
  } else {
    return false;
  }
}

// Handle Stop of Binary Logging
void LoggingConvertProcess(id_t state)
{
  SendRYLR("LOGGING STOPPED");

  // Open the Log File to Output Diagnostics
  LogFile = SD.open(FileName, O_RDONLY);
  if (!LogFile)
  {
    ErrorBlink(ERR_SD_FILE);
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
  digitalWrite(FIRE_PIN_A, LOW);
  digitalWrite(FIRE_PIN_B, LOW);

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

  // Close the Log File if Open
  if (LogFile)
  {
    LogFile.close();
  }

  // Stop SD Card Communication
  // Reset Log File Name
  SD.end();
  FileName = String();

  // Indicate Conversion is Complete
  digitalWrite(STATUS_PIN, LOW);

  SendRYLR("SAFING FIRESIDE");
}


// #### FAILURE State Checks and Processes
// Check why the System is in a Failure State
bool FailureCheck(id_t state)
{
  SendRYLR("FIRESIDE FAILURE");

  // Turn Off Igniters MOSFETS
  SendRYLR("TURNING OFF IGNITERS");
  digitalWrite(FIRE_PIN_A, LOW);
  digitalWrite(FIRE_PIN_B, LOW);

  // Indicate Igniter Status
  digitalWrite(STATUS_PIN, HIGH);
  SendRYLR("IGNITERS OFF");

  // Check SD Card Status
  // Reinitialize Dedicated SPI Interface to SD Card
  // Abort if Initialization Fails
  // Use Maximum SPI SCK Frequency Provided in SAMD21 Datasheet
  SendRYLR("CHECKING SDCARD");
  SD.end();
  if (!SD.begin(F_CPU / SPI_MIN_CLOCK_DIVIDER, SDCARD_SS_PIN))
  {
    ErrorBlink(ERR_SD_INIT);
  }

  // Disable DMA
  SendRYLR("DISABLING DMA");
  DMA.abort();

  // Wait for GroundSide Command
  if (RYLR.available() && ParseRYLR() == "SAFE")
  {
    // Only Proceed on Receipt of Safe Command
    return true;
  } else {
    // Rerun Diagnostics
    delay(100UL);
    return false;
  }
}

// Handle Safe State Transition After Failure
void FailureSafeProcess(id_t state)
{
  SendRYLR("SAFE COMMAND RECEIVED");
  SendRYLR("RESETTING TO SAFE");
}
