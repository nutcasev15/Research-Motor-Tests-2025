// #### Library Headers
// Arduino Framework and Data Types
#include <Arduino.h>

// Board IO Wiring Setup Wrappers
#include <wiring_private.h>

// SD Card Interface and File Class
#include <SD.h>


// #### Internal Headers
// Hardware Interface Definitions and Functions
#include "Interfaces.hpp"

// DMA Data Logging Function Prototypes
#include "DMADAQ.hpp"


// #### Internal Definitions
// Analog Pin Readout Buffer
uint16_t ReadoutBuffer[ADC_PARALLEL_CHANNELS];


// Compensated ADC DMA Buffer Block Length
// Align Block to SD Card 512 Byte Boundary to Optimise IO
// Compensate for Space for Timestamp Container
#define ADC_DMA_BLOCKLEN (ADC_PARALLEL_CHANNELS * 512 - sizeof(uint32_t))

// Circular DMA Buffer Data Storage Structure
// By Convention, Circular DMA Buffers are 2 Blocks Long
uint16_t DMABuffer[2 * ADC_DMA_BLOCKLEN];

// Pointer to Blocks in Circular DMA Buffer for SD Card SDWriting
// Updated in Half and Full Transfer Complete Callbacks
uint16_t *SDWriteBlockStart;

// Boolean to Track Current Block Write Status
volatile bool SDWriteBlockReady;

// Boolean to Signal SD Write Buffer Error
volatile bool SDWriteError;


// #### Hardware Configuration Functions
// ADC Module Configuration
void ConfigureADC(bool Continuous)
{
  ADC_ChannelConfTypeDef sConfig;

  // Select ADC Module One on MCU
  hadc1.Instance = ADC1;

  // Sync ADC to Core Clock: 80 MHz / 4 = 20 MHz
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;

  // Setup ADC Data Frame
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;

  // Enable Oversampling to Gain 2 Bits of Extra Resolution
  // Total Sampling Speed:
  // ADC Clock / (Oversampling Ratio * No. of Channels * Cycles per Sample)
  // See Channel Configuration for Cycles per Sample
  hadc1.Init.OversamplingMode = ENABLE;
  hadc1.Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_16;
  hadc1.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_2;
  hadc1.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  hadc1.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;

  // Instruct ADC to Scan Input Pins in Sequence
  hadc1.Init.NbrOfConversion = ADC_PARALLEL_CHANNELS;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;

  // Set Conversion Trigger to Internal Software Only
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;

  // Specify if ADC Should Continuously Scan the Input Pins
  // NOTE: DMA Should Not Be Used for Simple Pin Readout to Avoid Conflicts
  if (Continuous)
  {
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  } else {
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
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
  sConfig.SamplingTime = ADC_SAMPLETIME_24CYCLES_5;

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

  // Enable Clock to ADC Module After Setup
  __HAL_RCC_ADC_CLK_ENABLE();

  // Calibrate ADC in Single Ended Input Mode
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
}


// DMA Module Configuration
void ConfigureDMA(bool Continuous)
{
  // DMA Module Should Not Be Used in Single Shot Mode
  if (!Continuous)
  {
    return;
  }

  // Select Channel 1 on DMA Module One
  hdma_adc1.Instance = DMA1_Channel1;

  // Select Highest Priority Request
  hdma_adc1.Init.Request = DMA_REQUEST_0;
  hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;

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

  // Enable Clock to ADC Module One After Setup
  __HAL_RCC_DMA1_CLK_ENABLE();
}


// Successful Block One DMA Transfer Completion Callback
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  // Check If Previous Block is Still Being Written
  // Otherwise Check for Logging Finish Signal
  if (SDWriting || finished)
  {
    // Stop ADC Conversion
    HAL_ADC_Stop_DMA(&hadc1);

    // If Previous Block Write is Incomplete
    if (SDWriting)
    {
      // Signal SD Buffer Write Error
      SDWriteError = true;

      // Force Signal Logging Stop and Close File
      finished = true;
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
  if (SDWriting || finished)
  {
    // Stop ADC Conversion
    HAL_ADC_Stop_DMA(&hadc1);

    // If Previous Block Write is Incomplete
    if (SDWriting)
    {
      // Signal SD Buffer Write Error
      SDWriteError = true;

      // Force Signal Logging Stop and Close File
      finished = true;
    }
  }

  // Reset Block Write Pointer to 2nd Block in the Circular Buffer
  SDWriteBlockStart = &DMABuffer[ADC_DMA_BLOCKLEN];

  // Update Block Write Status for Logging Routine
  SDWriteBlockReady = true;
}


// Readout Analog Pins to Check Input
void ReadoutAnalogPins()
{
  // Configure ADC Module in Single Shot Mode
  // Do Not Use DMA for Single Shot Pin Readout
  ConfigureADC();

  // Disable ADC DMA to Avoid Conflicts
  HAL_ADC_Stop_DMA(&hadc1);

  // Wait for Conversion to Finish Sequentially on Each Input Pin
  // Save the Result in the Readout Buffer
  for (short channel = 0; channel < ADC_PARALLEL_CHANNELS; channel++)
  {
    // Start ADC for Single Scan of Input Pins
    HAL_ADC_Start(&hadc1);

    // Wait for Conversion Completion
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);

    // Save Converted Value
    ReadoutBuffer[channel] = (uint16_t)(HAL_ADC_GetValue(&hadc1));
  }

  // Stop ADC After Each Input Pin has been Read Out
  HAL_ADC_Stop(&hadc1);

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

    // Channel Value for 14-Bit Oversampled Data
    debug += (ReadoutBuffer[channel] * 3.3 / (1<<14));

    // Value Units and Separator
    debug += "V ";
  }

  // Transmit ADC Channel Debug Data over RYLR
  SendRYLR("ADC CHANNEL STATUS");
  SendRYLR(debug);
}


// Binary Log File and Initial DMA Buffer Configuration
void ConfigureLogging(String &Name)
{
  // Initialise Circular Buffer and Write Block Pointer
  // Log Data in 1st Block Initially
  SDWriteBlockStart = DMABuffer;
  memset(SDWriteBlockStart, 0X00, sizeof(DMABuffer));

  // Initialise Current Block Status for SD Card Write
  SDWriteBlockReady = false;

  // Initialise SD Write Buffer Error Signal Boolean
  SDWriteError = false;

  // Initialise Logging Status Boolean
  finished = false;

  // Select Logging File Path
  // Loop Until a Free Filename is Found
  // Avoid OverSDWriting Files
  String path;
  for (short id = 0; id >= 0; id++)
  {
    // Clear Existing Path
    path = "";

    // Build and Test Path
    path += id;
    path += ".dat";

    if (!SD.exists(path))
    {
      // Select Tested Path and Stop Loop
      break;
    }
  }

  // Open LogFile and Begin Log
  // Abort if File does not Open
  LogFile = SD.open(path, (O_CREAT | O_WRITE));
  if (!LogFile) {
    ErrorBlink(ERR_SD_FILE);
    return;
  }

  // Set Output File Name
  Name = path;

  return;
}


// Coupled ADC-DMA Transfer and Logging Trigger
void TriggerLogging()
{
  // Enable ADC and Trigger Conversion
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)DMABuffer, sizeof(DMABuffer));

  // Log Starting Time
  uint32_t start = micros();
  LogFile.write((const uint8_t *)&start, sizeof(uint32_t));
}


// Log Finalised Binary DMA Buffers to SD Card
void LogBuffers()
{
  // Check if DMA Handler Aborted
  if (SDWriteError)
  {
    // Force any Data in File Buffer to be Written to SD Card
    LogFile.flush();
    LogFile.close();

    // Indicate SD Write Buffer Error on LED
    ErrorBlink(ERR_SD_BUFF);

    // Abort Logging
    return;
  }

  // Check if SD Buffer Block is Ready For Write
  if (SDWriteBlockReady)
  {
    // Reset SD Write Block Status
    SDWriteBlockReady = false;

    // Set SD Card Write Flag
    SDWriting = true;

    // Write Timestamp to SD Card
    uint32_t time = micros();
    LogFile.write((const uint8_t *)&time, sizeof(uint32_t));

    // Dump Block to SD Card
    LogFile.write((const uint8_t *)SDWriteBlockStart, ADC_DMA_BLOCKLEN);

    // Reset SD Card Write Flag
    SDWriting = false;

    // Check if Finish Signal was Received from RYLR
    if (finished)
    {
      // Force any Data in File Buffer to be Written to SD Card
      LogFile.flush();
      LogFile.close();

      // Clear Circular DMA Buffer
      memset(DMABuffer, 0X00, sizeof(DMABuffer));
    }
  }
}


// Binary Log File to CSV File Converter
void ConvertLog(const String &Path)
{
  // Containers for CSV File and Associated Data
  File CSVFile;
  String CSVFileName, buffer;
  uint32_t start, end, progress;

  // Reserve Line Buffer Length
  buffer.reserve(256UL);

  // Check if Last Buffer was Written Successfully
  // Ensure Log File Data is Complete Before Conversion
  // Abort on Error
  if (SDWriting || LogFile)
  {
    ErrorBlink(ERR_SD_BUFF);
    return;
  } else {
    // Clear DMA Buffer for Conversion Purposes
    memset(DMABuffer, 0X00, sizeof(DMABuffer));
  }

  // Open Log File for Reading Only
  LogFile = SD.open(Path, O_RDONLY);

  // Check if Log File Opened Successfully
  if (!LogFile)
  {
    ErrorBlink(ERR_SD_FILE);
    return;
  }

  // Copy Log File Name for CSV File
  CSVFileName = LogFile.name();

  // Change File Extension to .csv
  CSVFileName.remove(CSVFileName.lastIndexOf('.'));
  CSVFileName += ".csv";

  // Create CSV File
  CSVFile = SD.open(CSVFileName, (O_CREAT | O_WRITE));

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

  // Start Reading Log File
  LogFile.seek(0UL);
  progress = 0UL;

  // Read Starting Time
  LogFile.read(&start, sizeof(uint32_t));

  // Iterate Through All Logged DMA Buffer Blocks
  do
  {
    // Clear Line Buffer
    buffer = "";

    // Read Timestamp from Log File
    LogFile.read(&end, sizeof(uint32_t));

    // Read Data from Log File into 1st Block of Circular Buffer
    LogFile.read(DMABuffer, ADC_DMA_BLOCKLEN);

    // Process Each ADC Sample in DMA buffer in Blocks
    for (uint16_t index = 0; index < ADC_DMA_BLOCKLEN; index += ADC_PARALLEL_CHANNELS)
    {
      // Calculate Timestamp for Current Sample Block
      buffer += (uint32_t)((end - start) / ADC_DMA_BLOCKLEN * index);

      // Deinterleave and Append ADC Sample Data to Buffer
      // NOTE: See Interfaces.hpp
      for (short channel = 0; channel < ADC_PARALLEL_CHANNELS; channel++)
      {
        buffer += ", ";
        buffer += DMABuffer[index + channel];
      }
    }

    // Update Timestamp and Write Buffer to CSV File
    start = end;
    CSVFile.println(buffer);

    // Send Progress Update on Significant Progress
    // Updates Sent to GroundSide Every 128 KB of Processed Data
    if ((LogFile.position() - progress) > 0X1FFFFUL)
    {
      progress = LogFile.position();

      // Clear and Reuse Line Buffer
      buffer = "";

      // Build Progress Report
      buffer += "PROGRESS: ";
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
