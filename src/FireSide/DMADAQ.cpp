// #### Library Headers
// Arduino Framework and Data Types
#include <Arduino.h>

// Board IO Wiring Setup Wrappers
#include <wiring_private.h>

// SD Card Interface and File Class
#include <SD.h>

// Adafruit ZeroDMA Class and Descriptors
#include <Adafruit_ZeroDMA.h>


// #### Internal Headers
// Hardware Interface Definitions and Functions
#include "Interfaces.hpp"

// DMA Data Logging Function Prototypes
#include "DMADAQ.hpp"


// #### Internal Definitions
// Compensated ADC DMA Buffer Length
// Align Buffer to SD Card 512 Byte Boundary to Optimise IO
// Compensate for Space for Timestamp and Buffer Status Containers
#define ADC_DMA_BUFFLEN (ADC_PARALLEL_CHANNELS * 512 - sizeof(uint32_t))

// Buffer Status Flags and Flag Masks
// Bit 0 and Bit 1 of uint32_t status - DMA Access Status
#define BUF_DMA_STATUS_MASK 0X03
// Buffer is not being used for DMA
#define BUF_DMA_INACTIVE 0x0
// DMA is Writing to Buffer
#define BUF_DMA_ACTIVE   0x1
// DMA has Finished Writing to Buffer
#define BUF_DMA_DONE     0x2

// Bit 2 and Bit 3 of uint32_t status - SD Write Status
#define BUF_SD_STATUS_MASK 0X0C
// Buffer is not Ready for SD Write
#define BUF_SD_WAITING 0x0
// Buffer is being Written to SD Card
#define BUF_SD_WRITING 0x4
// Buffer Data has been Written to SD Card
#define BUF_SD_DONE 0x8


// Adafruit ZeroDMA Descriptor
DmacDescriptor *DMAdesc;

// DMA Buffer Data Storage Structure
struct DMABuffer
{
  // Flag Container to Track Buffer Status
  uint32_t status;
  // Buffer Write Time Referenced to Controller Power Up
  uint32_t TimeStamp;
  // Buffer for DMA Transfer of ADC Results
  uint16_t DMA_ADCBuf[ADC_DMA_BUFFLEN];
} BufferA, BufferB;

// Pointers to DMA Buffers
// Pointer to Buffer Which Stores Currently Logging Data
DMABuffer *current;
// Temporary Pointer for Buffer Swapping
DMABuffer *swap;
// Pointer to Buffer Being Written to SD Card
DMABuffer *dump;

// Boolean to Signal SD Write Buffer Error
volatile bool DumpError;


// #### Hardware Configuration Functions
// ADC Module Configuration
void ConfigureADC()
{
  // Start ADC Module Setup
  // Flush ADC Pipeline
  ADC->SWTRIG.bit.FLUSH = 1;
  while (ADC->STATUS.bit.SYNCBUSY);

  // Setup ADC Module Clock
  // Supply GCLK3 8 MHz Output to ADC
  // See SAMD21 Datasheet for Clock Generator Data
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_ADC \
    | GCLK_CLKCTRL_GEN_GCLK3 \
    | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);

  // Setup ADC Internal Clock
  // ADC Max Clock is 2.1 MHz, Input Clock is 8 MHz
  // Set ADC Clock to 2 MHz
  ADC->CTRLB.bit.PRESCALER = ADC_CTRLB_PRESCALER_DIV4_Val;
  while (ADC->STATUS.bit.SYNCBUSY);

  // Setup Internal References to 0.5 * VDDANA = 1.65 V
  // Enable Reference Offset Compensation
  // Select 0.5 * VDDANA Reference
  ADC->REFCTRL.bit.REFCOMP = 1;
  ADC->REFCTRL.bit.REFSEL = ADC_REFCTRL_REFSEL_INTVCC1_Val;
  while (ADC->STATUS.bit.SYNCBUSY);

  // Setup ADC Operation Mode
  // Enable Freerunning Mode for Automatic DMA Transfer
  // Use Maximum ADC Resolution
  // NOTE: Only Positive Voltage Inputs are Allowed to the ADC
  ADC->CTRLB.bit.FREERUN = 1;
  ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_16BIT_Val;
  while (ADC->STATUS.bit.SYNCBUSY);

  // Scale Down Input to 0.5 * VDDANA Range to Reduce Noise
  // Use Internal Ground for 2nd Input
  ADC->INPUTCTRL.bit.GAIN = ADC_INPUTCTRL_GAIN_DIV2_Val;
  ADC->INPUTCTRL.bit.MUXNEG = ADC_INPUTCTRL_MUXNEG_GND_Val;
  while (ADC->STATUS.bit.SYNCBUSY);

  // Configure ADC I/O
  // We Use A3 For First Input Channel
  // Leaving A0 Free for DAC Output and Increment For The Next Channels
  // The Pins A3-A6 are Wired to the ADC Input Channels 4 Through 7
  // Necessary to use Input Scanning in the ADC Input Mux
  // Pins A1 and A2 are Wired to the ADC Input Channels 10 and 11
  // Input Channels 8 and 9 Will Also Be Sampled and Provide Junk Values
  // Refer: https://microchipsupport.force.com/s/article/
  // How-to-configure-input-scan-mode-of-ADC-module-in-SAMD10-D20-D21-R21-devices
  // Setup Input Pins as Analog
  pinPeripheral(A1, PIO_ANALOG); // ADC Input Channel 10
  pinPeripheral(A2, PIO_ANALOG); // ADC Input Channel 11
  pinPeripheral(A3, PIO_ANALOG); // ADC Input Channel 4
  pinPeripheral(A4, PIO_ANALOG); // ADC Input Channel 5
  pinPeripheral(A5, PIO_ANALOG); // ADC Input Channel 6
  pinPeripheral(A6, PIO_ANALOG); // ADC Input Channel 7

  // Select Pin For 1st Input
  ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[A3].ulADCChannelNumber;
  ADC->INPUTCTRL.bit.INPUTSCAN = ADC_PARALLEL_CHANNELS - 1;
  while (ADC->STATUS.bit.SYNCBUSY);

  // Enable ADC Interrupts on Result Conversion Completion
  // NOTE: Required for DMA Transfer to Trigger
  ADC->INTENSET.bit.RESRDY = 1;
  while (ADC->STATUS.bit.SYNCBUSY);
}


// Adafruit ZeroDMA Configuration
void ConfigureDMA()
{
  // Setup DMAC Module
  DMA.allocate();

  // Use ADC Interrupt as Transfer Trigger
  DMA.setTrigger(ADC_DMAC_ID_RESRDY);

  // Transfer One Unit on Each Trigger of ADC Interrupt
  DMA.setAction(DMA_TRIGGER_ACTON_BEAT);

  // Set Priority to Highest Available
  DMA.setPriority(DMA_PRIORITY_3);

  // Configure DMAC Channel for ADC DMA Transfers
  // NOTE: Destination is not Initialised Here
  // See ConfigureLogging() for Final Initialisation
  DMAdesc = DMA.addDescriptor(
    (void *)&ADC->RESULT.reg,
    NULL,
    sizeof(current->DMA_ADCBuf) / 2,
    DMA_BEAT_SIZE_HWORD,
    false,
    true
  );

  // Register Successful Transfer Handler
  DMA.setCallback(HandleTransferComplete, DMA_CALLBACK_TRANSFER_DONE);

  // Register Failed Transfer Handler
  DMA.setCallback(HandleTransferFail, DMA_CALLBACK_TRANSFER_ERROR);
}


// Successful DMA Transfer Handler
void HandleTransferComplete(Adafruit_ZeroDMA *)
{
  // Mark DMA to Buffer as Complete
  // Clear Previous DMA Status Flag
  current->status &= ~BUF_DMA_STATUS_MASK;
  current->status |= BUF_DMA_DONE;

  // Swap Data Buffer
  swap = current;

  // Signal SD Buffer Error if SD Buffer is Not Free
  if ((dump->status & BUF_SD_STATUS_MASK) == BUF_SD_WRITING)
  {
    // Disable ADC
    ADC->CTRLA.bit.ENABLE = 0;

    // Signal DMA Error to DAQ System
    DumpError = true;

    // Abort DMA and Exit Handler
    DMA.abort();
    return;
  }

  // Setup New Buffer for ADC DMA Transfers
  current = dump;

  // Queue New Buffer if Finish Signal Not Received
  if (!RYLR.available())
  {
    // Reconfigure ADC Transfer Descriptor
    DMA.changeDescriptor(
      DMAdesc,
      (void *)&ADC->RESULT.reg,
      (void *)&current->DMA_ADCBuf[ADC_DMA_BUFFLEN],
      sizeof(current->DMA_ADCBuf) / 2
    );

    // Mark DMA to Buffer as Active
    // Clear Previous DMA Status Flag
    current->status &= ~BUF_DMA_STATUS_MASK;
    current->status |= BUF_DMA_ACTIVE;

    // Mark Buffer as Waiting for DMA to Complete Before SD Write
    // Clear Previous SD Write Status Flag
    current->status &= ~BUF_SD_STATUS_MASK;
    current->status |= BUF_SD_WAITING;

    // Enable ADC DMA Transfers Again
    DMA.startJob();
  }
  else
  {
    // Disable ADC
    ADC->CTRLA.bit.ENABLE = 0;

    // Stop Further DMA Transfers
    DMA.abort();

    // Signal DAQ System to Stop Logging and Close File
    finished = true;
  }

  // Complete Swap and Mark Buffer for SD Write
  dump = swap;
}


// Failed DMA Transfer Handler
void HandleTransferFail(Adafruit_ZeroDMA *)
{
    // Disable ADC
    ADC->CTRLA.bit.ENABLE = 0;

    // Stop Further DMA Transfers
    DMA.abort();

    // Mark DMA to Buffer as Inactive
    // Clear Previous DMA Status Flag
    current->status &= ~BUF_DMA_STATUS_MASK;
    current->status |= BUF_DMA_INACTIVE;

    // Clear Previous SD Write Status Flag
    current->status &= ~BUF_SD_STATUS_MASK;

    // Signal DAQ System to Stop Logging and Close File
    finished = true;
}


// Binary Log File and Initial DMA Buffer Configuration
String ConfigureLogging()
{
  // Initialize Data Buffers and Buffer Pointers
  // Log Data in BufferA Initially
  current = &BufferA;
  dump = &BufferB;
  memset(current, 0X00, sizeof(DMABuffer));
  memset(dump, 0X00, sizeof(DMABuffer));

  // Initialize Handler Buffer Pointer
  swap = NULL;
  // Initialize SD Write Buffer Error Signal Boolean
  DumpError = false;
  // Initialize Logging Status Boolean
  finished = false;


  // Configure ADC DMA Transfer Descriptor
  DMA.changeDescriptor(
    DMAdesc,
    (void *)&ADC->RESULT.reg,
    (void *)&current->DMA_ADCBuf[ADC_DMA_BUFFLEN],
    sizeof(current->DMA_ADCBuf) / 2
  );


  // Select Logging File Path
  // Loop Until a Free Filename is Found
  // Avoid Overwriting Files
  String path;
  for (short id = 0; id >= 0; id++)
  {
    if (!SD.exists(String(id) + ".dat"))
    {
      path = String(id) + ".dat";
      break;
    }
  }

  // Open LogFile and Begin Log
  // Abort if File does not Open
  LogFile = SD.open(path, (O_CREAT | O_WRITE));
  if (!LogFile) {
    ErrorBlink(ERR_SD_FILE);
  }

  return path;
}


// Coupled ADC-DMA Transfer and Logging Trigger
void TriggerLogging()
{
  // Enable ADC and Trigger Freerunning Conversion
  ADC->CTRLA.bit.ENABLE = 1;
  ADC->SWTRIG.bit.START = 1;

  // Start DMA Transfer of Results from ADC
  DMA.startJob();

  // Mark DMA to Buffer as Active
  // Clear Previous DMA Status Flag
  current->status &= ~BUF_DMA_STATUS_MASK;
  current->status |= BUF_DMA_ACTIVE;

  // Mark Buffer as Waiting for DMA to Complete Before SD Write
  // Clear Previous SD Write Status Flag
  dump->status &= ~BUF_SD_STATUS_MASK;
  dump->status |= BUF_SD_WAITING;

  // Log Starting Time
  uint32_t start = micros();
  LogFile.write((const uint8_t *)&start, sizeof(uint32_t));
}


// Log Finalised Binary DMA Buffers to SD Card
void LogBuffers()
{
  // Check if DMA Handler Aborted
  if (DumpError)
  {
    // Force any Data in File Buffer to be Written to SD Card
    LogFile.flush();
    LogFile.close();

    // Indicate SD Write Buffer Error on LED
    ErrorBlink(ERR_SD_BUFF);
  }

  // Check if SD Buffer is Ready For Write
  if ((dump->status & BUF_DMA_STATUS_MASK) == BUF_DMA_DONE)
  {
    // Add Buffer Timestamp
    dump->TimeStamp = micros();

    // Mark SD Write from Buffer as Active
    // Clear Previous SD Write Status Flag
    dump->status &= ~BUF_SD_STATUS_MASK;
    dump->status |= BUF_SD_WRITING;

    // Dump Buffer to SD Card
    LogFile.write((const uint8_t *)dump, sizeof(DMABuffer));

    // Check if Finish Signal was Received from RYLR
    if (finished)
    {
      // Force any Data in File Buffer to be Written to SD Card
      LogFile.flush();
      LogFile.close();

      // Clear All Buffers
      memset(current, 0X00, sizeof(DMABuffer));
      memset(dump, 0X00, sizeof(DMABuffer));
    }
    else
    {
      // Clear ADC Data Array for DMA Transfer Handler
      memset(dump->DMA_ADCBuf, 0X00, sizeof(dump->DMA_ADCBuf));

      // Mark SD Write from Buffer as Complete
      // Clear Previous SD Write Status Flag
      dump->status &= ~BUF_SD_STATUS_MASK;
      dump->status |= BUF_SD_DONE;

      // Mark Buffer Ready For DMA
      // Clear Previous DMA Status Flag
      dump->status &= ~BUF_DMA_STATUS_MASK;
      dump->status |= BUF_DMA_INACTIVE;
    }
  }
}


// Binary Log File to CSV File Converter
void ConvertLog(String path)
{
  // Containers for CSV File and Associated Data
  File CSVFile;
  String CSVFileName, buffer;
  uint32_t time, progress;

  // Check if the Last Buffer was Written Successfully
  // Ensure the Log File Data is Complete Before Conversion
  // Abort on Error
  if ((dump->status & BUF_SD_STATUS_MASK) != BUF_SD_DONE)
  {
    ErrorBlink(ERR_SD_BUFF);
    return;
  } else {
    // Clear DMA Buffer for Conversion Purposes
    memset(dump, 0X00, sizeof(DMABuffer));
  }

  // Open the Log File for Reading Only
  LogFile = SD.open(path, O_RDONLY);
  // Copy Log File Name for CSV File
  CSVFileName = LogFile.name();
  // Change the File Extension to .csv
  CSVFileName.replace(String(".dat"), String(".csv"));
  // Create the CSV File
  CSVFile = SD.open(CSVFileName, (O_CREAT | O_WRITE));
  // Check if the log file opened successfully
  if (!LogFile)
  {
    ErrorBlink(ERR_SD_FILE);
    return;
  }

  // Prepare the CSV Header
  buffer = String("Time (us)");
  for (short channel = 0; channel < ADC_PARALLEL_CHANNELS; channel++)
  {
    buffer += ", A" + String(3 + channel);
  }
#if ADC_PARALLEL_CHANNELS > 4
  // Replace Channel Names for Higher Channel Counts
  // Label Junk Channels Correctly
  buffer.replace(String(", A7, A8"), String(", JUNK, JUNK"));
  buffer.replace(String("A9"), String("A1"));
  buffer.replace(String("A10"), String("A2"));
#endif
  // Write Header at Start of CSV file
  CSVFile.seek(0UL);
  CSVFile.println(buffer);

  // Start Reading Log File
  LogFile.seek(0UL);
  progress = 0UL;

  // Read Starting Time
  LogFile.read(&time, sizeof(uint32_t));

  // Iterate Through All Logged DMA Buffers
  do
  {
    // Read Data from the Log File into the DMA Buffer
    LogFile.read(dump, sizeof(DMABuffer));

    // Process Each ADC Sample in the DMA buffer in Blocks
    for (uint16_t index = 0; index < ADC_DMA_BUFFLEN; index += ADC_PARALLEL_CHANNELS)
    {
      // Calculate the Timestamp for the Current Sample Block
      buffer = String(
        static_cast<uint32_t>((dump->TimeStamp - time) \
        / ADC_DMA_BUFFLEN * index)
      );

      // Deinterleave and Append ADC Sample Data to the Buffer
      for (short channel = 0; channel < ADC_PARALLEL_CHANNELS; channel++)
      {
        buffer += ", " + String(dump->DMA_ADCBuf[index + channel]);
      }
    }

    // Update the Timestamp and Write the Buffer to CSV File
    time = dump->TimeStamp;
    CSVFile.println(buffer);

    // Send Progress Update on Significant Progress
    // Updates Sent to GroundSide Every 128 KB of Processed Data
    if ((LogFile.position() - progress) > 0X1FFFFUL)
    {
      progress = LogFile.position();
      SendRYLR(
        "PROGRESS: " \
        + String(progress) + " / " \
        + String(LogFile.size()) \
        + " BYTES"
      );
    }
  } while (LogFile.available());

  // Close the Binary Log and CSV Files
  LogFile.close();
  CSVFile.close();
}
