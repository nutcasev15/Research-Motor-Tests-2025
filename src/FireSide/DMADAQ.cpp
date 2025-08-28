// #### Library Headers
// Arduino Framework and Data Types
#include <Arduino.h>

// Board IO Wiring Setup Wrappers
#include <wiring_private.h>


// #### Internal Headers
// Hardware Interface Definitions and Functions
#include "Interfaces.hpp"

// DMA Data Logging Function Prototypes
#include "DMADAQ.hpp"


// #### Internal Definitions
// Analog Pin Readout Buffer
uint16_t ReadoutBuffer[ADC_PARALLEL_CHANNELS];


// ADC DMA Buffer Block Length
// Align Block to SD Card 512 Byte Boundary to Optimise IO
#define ADC_DMA_BLOCKLEN (ADC_PARALLEL_CHANNELS * 512)

// Circular DMA Buffer Data Storage Structure
// By Convention, Circular DMA Buffers are 2 Blocks Long
uint16_t DMABuffer[2 * ADC_DMA_BLOCKLEN];


// Pointer to Blocks in Circular DMA Buffer for SD Card SDWriting
// Updated in Half and Full Transfer Complete Callbacks
uint16_t *SDWriteBlockStart;

// Boolean to Track Current Block Write Status
volatile bool SDWriteBlockReady;

// Boolean to Track SD Write State
volatile bool SDWriting;

// Boolean to Track SD Logging Stop Signal
volatile bool SDLogStop;

// Boolean to Signal SD Write Buffer Error
volatile bool SDWriteError;


// ADC and DMA Interface using STM32 HAL
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;


// #### Hardware Configuration Functions
// DMA Module Configuration
void ConfigureDMA(bool Continuous)
{
  // DMA Module Should Not Be Used in Single Shot Mode
  if (!Continuous)
  {
    return;
  }

  // Enable Clock to DMA Module
  __HAL_RCC_DMA1_CLK_ENABLE();

  // Select Channel 1 on DMA Module One
  hdma_adc1.Instance = DMA1_Channel1;

  // Configure Transfer Direction and Address Increment
  // Only Memory Addresses Should Increment
  hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;

  // Inform DMA that ADC Data Frame is 16 Bit
  hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;

  // Specify DMA Buffer Usage
  // Use the Buffer in Circular Mode When ADC Runs Continuously
  hdma_adc1.Init.Mode = DMA_CIRCULAR;

  // Write Settings to DMA Module
  if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
  {
    ErrorBlink(ERR_HAL_DMA);
  }

  // Link DMA and ADC Modules
  __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

  // Setup DMA Global Interrupt
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}


// Handle DMA1 Channel1 Global Interrupt for ADC Callbacks
extern "C" void DMA1_Channel1_IRQHandler()
{
  HAL_DMA_IRQHandler(&hdma_adc1);
}


// Successful Block One DMA Transfer Completion Callback
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  // Check If Previous Block is Still Being Written
  // Otherwise Check for Logging Finish Signal
  if (SDWriting || SDLogStop)
  {
    // Stop ADC Conversion
    HAL_ADC_Stop_DMA(&hadc1);

    // If Previous Block Write is Incomplete
    if (SDWriting)
    {
      // Signal SD Buffer Write Error
      SDWriteError = true;
    }
  }

  // Reset Block Write Pointer to 1st Block in the Circular Buffer
  SDWriteBlockStart = DMABuffer;

  // Update Block Write Status for Logging Routine
  SDWriteBlockReady = true;
}


// Successful Block Two DMA Transfer Completion Callback
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  // Check If Previous Block is Still Being Written
  // Otherwise Check for Logging Finish Signal
  if (SDWriting || SDLogStop)
  {
    // Stop ADC Conversion
    HAL_ADC_Stop_DMA(&hadc1);

    // If Previous Block Write is Incomplete
    if (SDWriting)
    {
      // Signal SD Buffer Write Error
      SDWriteError = true;
    }
  }

  // Reset Block Write Pointer to 2nd Block in the Circular Buffer
  SDWriteBlockStart = &DMABuffer[ADC_DMA_BLOCKLEN];

  // Update Block Write Status for Logging Routine
  SDWriteBlockReady = true;
}


// ADC Conversion Error Callback
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
  UNUSED(hadc);

  ErrorBlink(ERR_HAL_ADC);
}


// ADC Module Configuration
void ConfigureADC(bool Continuous)
{
  ADC_ChannelConfTypeDef sConfig;

  // Enable Clock to ADC Module
  __HAL_RCC_ADC_CLK_ENABLE();

  // Select ADC Module One on MCU
  hadc1.Instance = ADC1;

  // Sync ADC to Core Clock: 80 MHz / 4 = 20 MHz
  // Max Allowable Clock at 12-Bit Resolution
  // See Page 384 in ST's RM0394 Manual For More Implementation Details
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;

  // Setup ADC 12-Bit Data Resolution
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;

  // Enable Oversampling to Reduce Signal Noise
  // Total Sampling Speed:
  // ADC Clock / (Oversampling Ratio * No. of Channels * Cycles per Sample)
  // See Channel Configuration for Cycles per Sample
  hadc1.Init.OversamplingMode = ENABLE;
  hadc1.Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_8;
  hadc1.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_3;

  // Instruct ADC to Scan Input Pins in Sequence
  hadc1.Init.NbrOfConversion = ADC_PARALLEL_CHANNELS;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;

  // Set Conversion Trigger to Internal Software Only
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;

  // Specify How the ADC Should Scan the Input Pins
  // NOTE: DMA Should Not Be Used for Simple Pin Readout to Avoid Conflicts
  // See Page 395 in ST's RM0394 Manual For More Implementation Details
  if (Continuous)
  {
    // Scan Continuously using DMA
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;

    // Ensure Discontinuous Mode is Disabled
    hadc1.Init.DiscontinuousConvMode = DISABLE;
  } else {
    // Disable Continuous Scanning to Avoid Overruns
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DMAContinuousRequests = DISABLE;

    // Enable Discontinuous Mode
    // Scan One Channel per Software Trigger
    hadc1.Init.DiscontinuousConvMode = ENABLE;
    hadc1.Init.NbrOfDiscConversion = 1;
  }

  // Write Settings to ADC Module
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    ErrorBlink(ERR_HAL_ADC);
  }

  // Configure ADC Channels
  // Set Single Ended Conversion with No Assumed Offset
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;

  // Configure Channel Sample Time
  // NOTE: Cycles per Sample = 12.5 + Sampling Cycles
  sConfig.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;

  // Loop Over All ADC Inputs and Write their Settings to the ADC
  // See Interfaces.hpp for ADC Hardware Setup Definition
  for (short input = 0; input < ADC_PARALLEL_CHANNELS; input++)
  {
    // Configure GPIO Input Pin to Analog Mode
    pinMode(ADCHardwareSetup[input].pin, INPUT_ANALOG);

    // Assign Hardware Input Channel to ADC Rank
    sConfig.Channel = ADCHardwareSetup[input].channel;
    sConfig.Rank = ADCHardwareSetup[input].rank;

    // Write Settings to Each ADC Input Channel
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
      ErrorBlink(ERR_HAL_ADC);
    }
  }

  // Setup ADC Global Interrupt
  // Select Lower Priority than DMA Channel Interrupt
  HAL_NVIC_SetPriority(ADC1_IRQn, 1, 1);
  HAL_NVIC_EnableIRQ(ADC1_IRQn);
}


// Handle ADC Global Interrupt for ADC Callbacks
extern "C" void ADC1_IRQHandler()
{
  HAL_ADC_IRQHandler(&hadc1);
}


// Readout Analog Pins to Check Input
void ReadoutAnalogPins()
{
  // Configure ADC Module in Single Shot Mode
  // Do Not Use DMA for Single Shot Pin Readout
  ConfigureADC();

  // Calibrate ADC in Single Ended Input Mode Before Converting
  // See Errata 2.6.10 in ST's ES0456 Errata Document for L412KBU6U
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
  {
    ErrorBlink(ERR_HAL_ADC);
  }

  // Wait for Conversion to Finish Sequentially on Each Input Pin
  // Save the Result in the Readout Buffer
  for (short channel = 0; channel < ADC_PARALLEL_CHANNELS; channel++)
  {
    // Start ADC for Single Scan of Input Pins
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
      ErrorBlink(ERR_HAL_ADC);
    }

    // Wait 100 ms for Conversion Completion
    if (HAL_ADC_PollForConversion(&hadc1, 100UL) != HAL_OK)
    {
      ErrorBlink(ERR_HAL_ADC);
    }

    // Save Converted Value
    ReadoutBuffer[channel] = (uint16_t)(HAL_ADC_GetValue(&hadc1));
  }

  // Stop ADC After Each Input Pin has been Read Out
  if (HAL_ADC_Stop(&hadc1) != HAL_OK)
  {
    ErrorBlink(ERR_HAL_ADC);
  }

  // Assemble ADC Calibration Data and Channel Debug Data
  String debug = "Calibration=";
  debug += HAL_ADCEx_Calibration_GetValue(&hadc1, ADC_SINGLE_ENDED);
  debug += ' ';

  for(short channel = 0; channel < ADC_PARALLEL_CHANNELS; channel++)
  {
    // Channel Label
    // NOTE: See Interfaces.hpp
    debug += 'A';
    debug += channel;

    // Separator
    debug += '=';

    // Channel Value for 12-Bit Data
    debug += (ReadoutBuffer[channel] * 3.3 / (1<<12));

    // Value Units and Separator
    debug += "V ";
  }

  // Transmit ADC Channel Debug Data over RYLR
  SendRYLR("ADC CHANNEL STATUS");
  SendRYLR(debug);
}


// Binary Logfile Name Helper
String GetLogfileName(bool Initialise)
{
  static String FileName = "";

  if (FileName.length() == 0 && Initialise)
  {
    // Select Fresh Logging File Path if Blank
    // Loop Until a Free Filename is Found
    // Avoid Overwriting Files
    for (short id = 0; id >= 0; id++)
    {
      // Build and Test Path
      FileName = id;
      FileName += ".dat";

      if (!SD.exists(FileName))
      {
        // Select Tested Path and Stop Loop
        break;
      }
    }
  }

  // If File Name is not Blank or Initialisation is Disabled
  // Returns File Name without Changes
  // Otherwise Returns a New File Name
  return FileName;
}


// Binary Logfile and Initial DMA Buffer Configuration
void ConfigureLogging()
{
  // Initialise Circular Buffer and Write Block Pointer
  // Log Data in 1st Block Initially
  SDWriteBlockStart = DMABuffer;
  memset(SDWriteBlockStart, 0X00, sizeof(DMABuffer));

  // Initialise Current Block Status for SD Card Write
  SDWriteBlockReady = false;

  // Initialise SD Logging Stop Signal Boolean
  SDLogStop = false;

  // Initialise SD Write Buffer Error Signal Boolean
  SDWriteError = false;

  // Select a Fresh Filename for the Binary Logfile
  GetLogfileName(true);
}


// Coupled ADC DMA Transfer and Logging Trigger
void TriggerLogging()
{
  // Ensure Completion of Outgoing RYLR Communications
  RYLR.flush();

  // Empty Received Data in RYLR Communications Buffer
  // Remove Chances of Premature Logging Termination
  while (RYLR.available())
  {
    RYLR.read();
  }

  // Calibrate ADC in Single Ended Input Mode Before Triggering
  // See Errata 2.6.10 in ST's ES0456 Errata Document for L412KBU6U
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
  {
    ErrorBlink(ERR_HAL_ADC);
  }

  // Enable ADC and Trigger Conversion
  // Account for 2 Byte Size of Each ADC Sample
  HAL_ADC_Start_DMA(
    &hadc1,
    (uint32_t *)DMABuffer,
    sizeof(DMABuffer) / sizeof(uint16_t)
  );
}


// Log Finalised Binary DMA Buffers to SD Card
void LogBuffersinLoop()
{
  // Create Binary Logfile on SD Card
  File LogFile = SD.open(GetLogfileName(), FILE_WRITE);

  // Abort if File is not Open
  if (!LogFile) {
    ErrorBlink(ERR_SD_FILE);
  }

  // Start Logging Loop
  // Stop Loop on Receipt of Newline Character
  do {
    // Check if DMA Handler Aborted
    if (SDWriteError)
    {
      // Close File on SD Card After Logging Loop
      LogFile.close();

      // Indicate SD Write Buffer Error on LED
      ErrorBlink(ERR_SD_BUFF);
    }

    // Check if SD Buffer Block is Ready For Write
    if (SDWriteBlockReady)
    {
      // Reset SD Write Block Status
      SDWriteBlockReady = false;

      // Set SD Card Write Flag
      SDWriting = true;

      // Dump Block to SD Card
      // NOTE: Each ADC Sample in Block is 2 Bytes
      LogFile.write(
        (const uint8_t *)SDWriteBlockStart,
        ADC_DMA_BLOCKLEN * sizeof(uint16_t)
      );

      // Write Timestamp to SD Card
      uint32_t time = micros();
      LogFile.write((const uint8_t *)&time, sizeof(uint32_t));

      // Reset SD Card Write Flag
      SDWriting = false;
    }
  } while (RYLR.read() != '\n');

  // Close File on SD Card After Logging Loop
  LogFile.close();

  // Finish Signal was Received from RYLR
  // Signal Stop of Data Logging on SD Card for ADC Callbacks
  SDLogStop = true;

  // Clear Circular DMA Buffer
  memset(DMABuffer, 0X00, sizeof(DMABuffer));
}


// Binary Logfile to CSV File Converter
void ConvertLog(const String &Path)
{
  // Containers for Files and Associated Data
  File CSVFile, LogFile;
  String CSVFileName, buffer;
  uint32_t StartTime, EndTime, progress;

  // Reserve Line Buffer Length
  buffer.reserve(64UL);

  // Clear DMA Buffer for Conversion Purposes
  memset(DMABuffer, 0X00, sizeof(DMABuffer));

  // Open Logfile for Reading Only
  LogFile = SD.open(Path, FILE_READ);

  // Check if Logfile Opened Successfully
  if (!LogFile)
  {
    ErrorBlink(ERR_SD_FILE);
    return;
  } else {
    // Output Logfile Diagnostics
    SendRYLR("FILENAME: " + String(LogFile.name()));
    SendRYLR("FILESIZE: " + String(LogFile.size()));
  }

  // Copy Logfile Name for CSV File
  CSVFileName = LogFile.name();

  // Change File Extension to .csv
  CSVFileName.remove(CSVFileName.lastIndexOf('.'));
  CSVFileName += ".csv";

  // Attempt to Open CSV File
  CSVFile = SD.open(CSVFileName, FILE_WRITE);

  // Only Proceed if CSV Logfile is Blank and Ready for Conversion
  if (!CSVFile || CSVFile.size() > 0) {
    // Close all Files and Abort
    LogFile.close();
    CSVFile.close();
    return;
  }

  // Reset Line Buffer and Prepare CSV Header
  // NOTE: See Interfaces.hpp
  buffer = "Time (us)";
  for (short channel = 0; channel < ADC_PARALLEL_CHANNELS; channel++)
  {
    buffer += ", A";
    buffer += channel;
  }

  // Write Header at Start of CSV file
  CSVFile.seek(0UL);
  CSVFile.println(buffer);

  // Start Reading Logfile
  LogFile.seek(0UL);
  StartTime = progress = 0UL;

  // Iterate Through All Logged DMA Buffer Blocks
  do
  {
    // Read Data from Logfile into 1st Block of Circular Buffer
    // Account for 2 Byte Width of Each ADC Sample
    LogFile.read(DMABuffer, ADC_DMA_BLOCKLEN * sizeof(uint16_t));

    // Read Timestamp from Logfile
    LogFile.read(&EndTime, sizeof(uint32_t));

    // Load Starting Timestamp if Blank
    if (!StartTime)
    {
      StartTime = EndTime;

      // Discard First Buffer's Data
      continue;
    }

    // Process Each ADC Sample in DMA buffer in Rows
    for (uint16_t index = 0; index < ADC_DMA_BLOCKLEN; index += ADC_PARALLEL_CHANNELS)
    {
      // Clear Line Buffer
      buffer = ' ';

      // Calculate Timestamp for Current Row of Samples
      buffer += (uint32_t)(
        (((EndTime - StartTime) * index) / ADC_DMA_BLOCKLEN) + StartTime
      );

      // Deinterleave and Append ADC Sample Data to Buffer
      // NOTE: See Interfaces.hpp
      for (short channel = 0; channel < ADC_PARALLEL_CHANNELS; channel++)
      {
        buffer += ", ";
        buffer += DMABuffer[index + channel];
      }

      // Write Buffer to CSV File
      CSVFile.println(buffer);
    }

    // Update Timestamp for Next Block
    StartTime = EndTime;

    // Send Progress Update on Significant Progress
    // Updates Sent to GroundSide Every 128 KB of Processed Data
    if ((LogFile.position() - progress) > 0X1FFFFUL)
    {
      progress = LogFile.position();

      // Build Progress Report
      buffer = "PROGRESS: ";
      buffer += progress;
      buffer += " / ";
      buffer += LogFile.size();
      buffer += " BYTES";

      // Send Progress Report
      SendRYLR(buffer);
    }
  } while (LogFile.available());

  // Close Binary Log and CSV Files
  LogFile.close();
  CSVFile.close();
}
