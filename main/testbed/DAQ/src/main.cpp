/// https://gcc.gnu.org/onlinedocs/gcc/Function-Specific-Option-Pragmas.html#Function-Specific-Option-Pragmas
// Optimize System Wide For Maximum Performance
#pragma GCC optimize("Ofast")

// ####################################################################### Libraries

// Include Arduino Framework
#include <Arduino.h>
// Include Board Wiring Header
#include <wiring_private.h>

// Include Custom SDFat Library From https://github.com/nutcasev15/SdFat#HWDrv
// Override exFAT LFN Setting to Disable Long File Names << Reduce Library Footprint and Improve FS Speed
#define USE_LONG_FILE_NAMES 0

// Override Free Cluster Journaling Setting << Enable Journaling For Faster Buffer Management
#define MAINTAIN_FREE_CLUSTER_COUNT 1

// Override SPI CRC Checking Setting << Enable Table Based CRC Checks
#define USE_SD_CRC 2

// Include SdFat and SPI Headers
#include <SPI.h>
#include <SdFat.h>

// ####################################################################### HW Configuration Definitions

// Define Target ADC Sampling Frequency (Hz)
#define TARGET_ADC_FREQ 5000

// Define SD Card Write Frequency (Hz)
#define TARGET_SD_WRITE_FREQ 10

// Define DMA Buffer End Extra Padding (Unit is HWORD - 16 Bits For One 13 Bit Converted Sample)
#define BUF_END_PAD 0

// Define Number of Concurrent ADC Channels
#define ADC_PARALLEL_CHANNELS 8

// Define Final ADC-DMA Buffer Length
#define ADC_DMA_BUFLEN ADC_PARALLEL_CHANNELS * (TARGET_ADC_FREQ / TARGET_SD_WRITE_FREQ + BUF_END_PAD)

// Define Logging Finish Indication Pin Where Button is Connected
#define N_ENABLE 7
#define ACK      6

// Define Boolean to Track Finish Button Press
volatile bool finished;

// Define Boolean to Signal SD Write Buffer Error
volatile bool sdbuf_err;

// Define ADC-DMA Channel ID and Priority
#define ADC_DMA_CH_ID 0
#define ADC_DMA_CH_PRIO DMAC_CHCTRLB_LVL_LVL0_Val // Select Highest Priority Level

// Define DMAC Channel Descriptor and Writeback Descriptor
volatile DmacDescriptor dmadesc __attribute__((aligned(16)));
volatile DmacDescriptor dmawrbk __attribute__((aligned(16)));

// Define Data Storage Structure with DMA Buffer
struct DataBuf
{
  uint32_t timestamp;                 // Stores Time of SD Write From Controller Power Up
  int16_t DMA_ADCBuf[ADC_DMA_BUFLEN]; // Define Buffer for DMA Transfer of ADC Results
  uint32_t bufstat;                   // Define Flag Container to Track Buffer Status
} Buf1, Buf2;                         // Define 1st Buffer for ADC-DMA Writes and 2nd Buffer for SD Card Writes

// Define Buffer Status Flags and Flag Masks
// Bit 0 and Bit 1 of uint32_t bufstat - DMA Access Status
#define BUF_DMA_INACTIVE 0 // Buffer is not being used for DMA
#define BUF_DMA_ACTIVE 1   // DMA is Writing to Buffer
#define BUF_DMA_DONE 2     // DMA has Finished Writing to Buffer
#define BUF_DMA_STATUS_MASK 0X03
#define BUF_DMA_STATUS_POS 0

// Bit 2 and Bit 3 of uint32_t bufstat - SD Write Status
#define BUF_SD_WAITING 0 // Buffer is not Ready for SD Write
#define BUF_SD_WRITING 1 // Buffer is being Written to SD Card
#define BUF_SD_WRITTEN 2 // Buffer Data has been Written to SD Card
#define BUF_SD_STATUS_MASK 0X0C
#define BUF_SD_STATUS_POS 2

// Define Pointers to Buffers
DataBuf *curbuf; // Pointer to Buffer Which Stores Currently Logging Data
DataBuf *isrbuf; // Temporary Pointer to Buffer Which is Being Swapped in the DMAC Handler ISR
DataBuf *sdbuf;  // Pointer to Buffer Which is Being Written to SD Card

// Define SDFat Object for SD Card Interface
SdFat SD;

// Define File Object for SD Card File System
File LogFile;

// ####################################################################### HW Error Definitions
#define ERR_SD_INIT 1      // Throw this if SD Card Initialization Fails
#define ERR_SD_FILE 2      // Throw this if LogFile is not Available
#define ERR_SD_BUF 3       // Throw this if SD Data Buffer is not Free When DMAC Handler ISR is Called
#define ERR_LOG_FINISHED 4 // Throw this if Finish Button is Pressed

// ####################################################################### HW Configuration Functions

// ###################################################################### Finite State Machine Definition
typedef enum {
  STANDBY,
  ACQUIRE,
  FINISH,
  FAILURE
} STATE;
STATE currentState = STANDBY;

// Error Indicator Function
void error_blink(uint8_t CODE)
{
  // Error Blink Repeating Interval
  const uint16_t period_ms = 5000;

  // DEBUG
  // Serial.println(CODE);

  // Turn Indicator LED Off
  digitalWrite(LED_BUILTIN, LOW);
  // Blink CODE Number of Times, Then Wait and Repeat
  while (1)
  {
    for (short i = 0; i < CODE; i++)
    {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(period_ms / (2 * CODE));
      digitalWrite(LED_BUILTIN, LOW);
      delay(period_ms / (2 * CODE));
    }
    delay(period_ms / 2);
  }
}

// ADC Configuration Function
void adc_config()
{
  // Configure PM To Provide Power to ADC Module
  PM->APBCMASK.bit.ADC_ = 1;

  // Setup ADC Module Clock
  while (GCLK->STATUS.bit.SYNCBUSY);                                                  // Wait Until Sync
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_ADC | GCLK_CLKCTRL_GEN(3) | GCLK_CLKCTRL_CLKEN; // Supply GCLK3 8 MHz Output to ADC
  while (GCLK->STATUS.bit.SYNCBUSY);                                                  // Wait Until Sync

  // Start ADC Module Setup
  // Flush ADC Pipeline
  ADC->SWTRIG.bit.FLUSH = 1;
  // Disable ADC
  ADC->CTRLA.bit.ENABLE = 0;
  // Reset ADC
  ADC->CTRLA.bit.SWRST = 1;
  while (ADC->STATUS.bit.SYNCBUSY); // Wait Until Sync

  // Setup ADC Internal Clock
  // ADC Max Clock is 2.1 MHz, Input Clock is 8 MHz, Hence, We Divide by 4
  ADC->CTRLB.bit.PRESCALER = ADC_CTRLB_PRESCALER_DIV4_Val; // Set ADC Clock to 2 MHz
  while (ADC->STATUS.bit.SYNCBUSY); // Wait Until Sync

  // Setup Internal References to 0.5 * VDDANA = 1.65 V
  ADC->REFCTRL.bit.REFCOMP = 1;                             // Enable Reference Offset Compensation
  ADC->REFCTRL.bit.REFSEL = ADC_REFCTRL_REFSEL_INTVCC1_Val; // Select 0.5 * VDDANA Reference
  while (ADC->STATUS.bit.SYNCBUSY); // Wait Until Sync

  // Setup ADC Operation Mode
  // Differential Mode is Enabled as Piezos Produce Negative Voltages
  // Freerunning Mode is Enabled for Automatic DMA Transfer to Buffers
  ADC->CTRLB.bit.DIFFMODE = 1;
  ADC->CTRLB.bit.FREERUN = 1;
  ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_16BIT_Val; // Use 16 Bits to Account For Sample Accumulation Where Each Sample is 13 Bits
  while (ADC->STATUS.bit.SYNCBUSY); // Wait Until Sync

  // Setup Sampling Parameters
  // See 1st Calculator Here: https://blog.thea.codes/getting-the-most-out-of-the-samd21-adc/
  // We Oversample by 4 Times, Then Right Shift the Result to Gain One Extra Bit Resolution
  // This Extra Bit Compensates for the Sign Bit Required to Use the ADC in Differential Mode
  // We Want Sampling Time < 25us, So That All Channels can be Serviced at > 5000 Hz
  // The Target Sampling Time is < 16.67us, After Applying a Safety Factor of 1.5
  ADC->AVGCTRL.bit.SAMPLENUM = ADC_AVGCTRL_SAMPLENUM_4_Val; // Accumulate 4 Samples per Conversion
  ADC->AVGCTRL.reg |= ADC_AVGCTRL_ADJRES(1);                // Divide Result by 2 to Recover One Extra Bit of Information
  ADC->SAMPCTRL.bit.SAMPLEN = 10;
  while (ADC->STATUS.bit.SYNCBUSY); // Wait Until Sync

  // Configure ADC I/O
  // We Use A3 For First Input Channel, Leaving A0 Free for DAC Output and Increment For The Next Channels
  // The Pins A3-A6 are Wired to the ADC Input Channels 4 Through 7, Which is Necessary to use Input Scanning in the ADC Input Mux
  // Pins A1 and A2 are Wired to the ADC Input Channels 10 and 11, While Input Channels 8 and 9 Will Also Be Sampled and Provide Junk Values
  // Refer: https://microchipsupport.force.com/s/article/How-to-configure-input-scan-mode-of-ADC-module-in-SAMD10-D20-D21-R21-devices
  ADC->INPUTCTRL.bit.GAIN = ADC_INPUTCTRL_GAIN_DIV2_Val;    // Scale Down Input to 0.5 * VDDANA Range
  ADC->INPUTCTRL.bit.MUXNEG = ADC_INPUTCTRL_MUXNEG_GND_Val; // Use Internal Ground for 2nd Input
  // Setup Input Pins as Analog
  pinPeripheral(A1, PIO_ANALOG); // ADC Input Channel 10
  pinPeripheral(A2, PIO_ANALOG); // ADC Input Channel 11
  pinPeripheral(A3, PIO_ANALOG); // ADC Input Channel 4
  pinPeripheral(A4, PIO_ANALOG); // ADC Input Channel 5
  pinPeripheral(A5, PIO_ANALOG); // ADC Input Channel 6
  pinPeripheral(A6, PIO_ANALOG); // ADC Input Channel 7
  ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[A3].ulADCChannelNumber; // Select Pin For 1st Input
  ADC->INPUTCTRL.bit.INPUTSCAN = ADC_PARALLEL_CHANNELS - 1;             // Scan the Next Channels As Well
  while (ADC->STATUS.bit.SYNCBUSY); // Wait Until Sync

  // Enable ADC Interrupts on Errors and Result Ready, Clear The Rest
  ADC->INTENSET.bit.OVERRUN = 1; // Enable Interrupt on Overrun
  ADC->INTENSET.bit.RESRDY = 1;  // Enable Interrupt on Conversion Complete, Required for DMA to Trigger
  ADC->INTENCLR.bit.SYNCRDY = 1; // Disable Interrupt on Sync Complete
  ADC->INTENCLR.bit.WINMON = 1;  // Disable Window Monitor Interrupt
  while (ADC->STATUS.bit.SYNCBUSY); // Wait Until Sync

  // Copy Over ADC Factory Calibration Values
  // Refer: https://github.com/arduino/ArduinoCore-samd/blob/master/cores/arduino/startup.c
  // Setup ADC Bias Calibration
  uint32_t bias = (*((uint32_t *)ADC_FUSES_BIASCAL_ADDR) & ADC_FUSES_BIASCAL_Msk) >> ADC_FUSES_BIASCAL_Pos;
  // Set ADC Linearity bits 4:0
  uint32_t linearity = (*((uint32_t *)ADC_FUSES_LINEARITY_0_ADDR) & ADC_FUSES_LINEARITY_0_Msk) >> ADC_FUSES_LINEARITY_0_Pos;
  // Set ADC Linearity bits 7:5
  linearity |= ((*((uint32_t *)ADC_FUSES_LINEARITY_1_ADDR) & ADC_FUSES_LINEARITY_1_Msk) >> ADC_FUSES_LINEARITY_1_Pos) << 5;
  ADC->CALIB.reg = ADC_CALIB_BIAS_CAL(bias) | ADC_CALIB_LINEARITY_CAL(linearity);
  while (ADC->STATUS.bit.SYNCBUSY); // Wait Until Sync
}

// DMAC Configuration Function
void dmac_config()
{
  // Configure PM To Provide Power to DMAC Module
  PM->AHBMASK.bit.DMAC_ = 1;
  PM->APBBMASK.bit.DMAC_ = 1;

  // Start DMAC Setup
  // Disable DMAC
  DMAC->CTRL.bit.DMAENABLE = 0;
  DMAC->CTRL.bit.CRCENABLE = 0;
  // Reset DMAC
  DMAC->CTRL.bit.SWRST = 1;

  // Initialize Descriptors in SRAM
  memset((void *)&dmadesc, 0X00, sizeof(dmadesc));
  memset((void *)&dmawrbk, 0X00, sizeof(dmawrbk));

  // Configure DMAC Address Space
  DMAC->BASEADDR.reg = (uint32_t)&dmadesc; // Start Address of Channel Descriptor
  DMAC->WRBADDR.reg = (uint32_t)&dmawrbk;  // Start Address of Writeback Descriptor

  // Configure DMAC Channel and Transfer Priorities
  DMAC->CTRL.bit.LVLEN0 = 1;                                                                      // Enable Highest Priority Level for ADC-DMA Channel Use
  DMAC->QOSCTRL.reg = DMAC_QOSCTRL_DQOS_HIGH | DMAC_QOSCTRL_FQOS_HIGH | DMAC_QOSCTRL_WRBQOS_HIGH; // Set DMAC Bus Priorities to High

  // Initialize Data Buffers and Buffer Pointers
  curbuf = &Buf1; // Log Data in Buf1 Initially
  sdbuf = &Buf2;
  isrbuf = NULL; // Initialize DMAC Handler ISR Buffer Pointer
  memset(curbuf, 0X00, sizeof(DataBuf));
  memset(sdbuf, 0X00, sizeof(DataBuf));

  // Configure DMAC Channel for ADC-DMA
  DMAC->CHID.bit.ID = DMAC_CHID_ID(ADC_DMA_CH_ID);
  DMAC->CHCTRLA.bit.SWRST = 1;                               // Reset Channel
  DMAC->CHCTRLA.bit.ENABLE = 0;                              // Disable Channel
  DMAC->CHCTRLB.bit.LVL = ADC_DMA_CH_PRIO;                   // Set Channel Priority Level
  DMAC->CHCTRLB.bit.TRIGACT = DMAC_CHCTRLB_TRIGACT_BEAT_Val; // Transfer One Half Word Per Request from ADC
  DMAC->CHCTRLB.bit.TRIGSRC = ADC_DMAC_ID_RESRDY;            // Hook ADC Into DMA Channel

  // Configure ADC-DMA Channel Descriptor
  dmadesc.BTCTRL.bit.BEATSIZE = DMAC_BTCTRL_BEATSIZE_HWORD_Val; // Set Transfer Quantum to Half Word AKA 16 Bit
  dmadesc.BTCTRL.bit.BLOCKACT = DMAC_BTCTRL_BLOCKACT_NOACT_Val; // Take No Action After Transaction Completes, DMAC Handler Will Takeover
  dmadesc.BTCTRL.bit.DSTINC = 1;                                // Enable Destination Address Increment
  dmadesc.BTCTRL.bit.STEPSEL = DMAC_BTCTRL_STEPSEL_DST_Val;     // Configure Stepsize For Destination Increment
  dmadesc.BTCTRL.bit.STEPSIZE = DMAC_BTCTRL_STEPSIZE_X1_Val;    // Transfer in Contiguous Memory Chunks
  dmadesc.BTCTRL.bit.VALID = 0;                                 // Invalidate Descriptor
  dmadesc.BTCNT.reg = sizeof(curbuf->DMA_ADCBuf) / 2;           // Set Maximum Holding Capacity of Buffer
  dmadesc.DESCADDR.reg = 0x00;                                  // Set Descriptor as First and Final Descriptor
  dmadesc.SRCADDR.reg = (uint32_t)&ADC->RESULT.reg;             // Set ADC Result Register as Source
  dmadesc.DSTADDR.reg = 0x00;                                   // Initialize Destination Address, To Be Modified Later

  // Configure ADC-DMA Channel Transaction Complete Interrupt
  NVIC_EnableIRQ(DMAC_IRQn);      // Enable NVIC Call of DMAC Handler ISR
  DMAC->CHINTENSET.bit.TCMPL = 1; // Call Handler on Transaction Completion
}

// ####################################################################### DMAC Handler ISR Function
void DMAC_Handler()
{
  DMAC->CHID.bit.ID = DMAC_CHID_ID(ADC_DMA_CH_ID); // Select ADC-DMA Channel
  DMAC->CHCTRLA.bit.ENABLE = 0;                    // Disable ADC-DMA Channel

  // Acknowledge Transaction Completion INterrupt
  DMAC->CHINTFLAG.bit.TCMPL = 1;

  // Mark DMA to Buffer as Complete
  curbuf->bufstat &= ~BUF_DMA_STATUS_MASK;               // Clear Previous DMA Status Flag
  curbuf->bufstat |= BUF_DMA_DONE << BUF_DMA_STATUS_POS; // Mark DMA Write to Buffer as Complete

  // Swap Data Buffer
  isrbuf = curbuf;

  // Signal SD Buffer Error if SD Buffer is Not Free
  if ((sdbuf->bufstat & BUF_SD_STATUS_MASK) == (BUF_SD_WRITING << BUF_SD_STATUS_POS))
  {
    // Disable ADC and DMAC
    DMAC->CTRL.bit.DMAENABLE = 0;
    ADC->CTRLA.bit.ENABLE = 0;

    // Signal DMA Error to DAQ System Loop
    sdbuf_err = true;

    // Abort Handler
    return;
  }

  // Setup New Buffer for ADC-DMA Transfer
  curbuf = sdbuf;

  // Queue New Buffer if Finish Button was not Pressed
  // If Finish Button was Pressed, Stop Handler Calls and Signal End of Logging
  if (!digitalRead(N_ENABLE))
  {
    // Reconfigure ADC-DMA Channel Descriptor
    dmadesc.BTCTRL.bit.VALID = 0;                                        // Invalidate Descriptor
    dmadesc.BTCNT.reg = sizeof(curbuf->DMA_ADCBuf) / 2;                  // Set Maximum Holding Capacity of Buffer
    dmadesc.DSTADDR.reg = (uint32_t)&curbuf->DMA_ADCBuf[ADC_DMA_BUFLEN]; // Initialize Destination Address to Last Address in DMA Buffer
    dmadesc.BTCTRL.bit.VALID = 1;                                        // Validate Descriptor

    // Mark DMA to Buffer as Active
    curbuf->bufstat &= ~BUF_DMA_STATUS_MASK;                 // Clear Previous DMA Status Flag
    curbuf->bufstat |= BUF_DMA_ACTIVE << BUF_DMA_STATUS_POS; // Mark DMA Write to Buffer as Active

    // Mark Buffer as Waiting for DMA to Complete Before SD Write
    curbuf->bufstat &= ~BUF_SD_STATUS_MASK;                 // Clear Previous SD Write Status Flag
    curbuf->bufstat |= BUF_SD_WAITING << BUF_SD_STATUS_POS; // Set SD Write Status as Waiting

    // Enable ADC-DMA Channel
    DMAC->CHCTRLA.bit.ENABLE = 1;
  }
  else
  {
    // Disable ADC and DMAC
    DMAC->CTRL.bit.DMAENABLE = 0;
    ADC->CTRLA.bit.ENABLE = 0;

    // Signal DAQ System Loop to Stop Logging and Close File
    finished = true;
  }

  // Setup ISR Handler Buffer for SD Write
  sdbuf = isrbuf;
}

// ####################################################################### DAQ Operations Initialization Function
void setup()
{
  // DEBUG
  // Serial.begin(115200);
  // while (!Serial);
  // analogWrite(A0, 343);

  // Setup Finish Pin and LED Status Indicator
  pinMode(LED_BUILTIN, OUTPUT);            // For Operation Status Indication
  digitalWrite(LED_BUILTIN, LOW);          // Turn Off LED
  pinMode(N_ENABLE, INPUT_PULLDOWN); // Setup Digital Pin for Signal Logging Finish
  pinMode(ACK, OUTPUT);
  digitalWrite(ACK, HIGH);

  sdbuf_err = false; // Initialize SD Write Buffer Error Signal Boolean
  finished = false;  // Initialize Logging Status Boolean

  // Initialize Dedicated SDFat SPI Interface to SD Card
  // Abort if Initialization Fails
  if (!SD.begin(SdSpiConfig(SDCARD_SS_PIN, DEDICATED_SPI, SD_SCK_MHZ(12)))) // Use Maximum SPI SCK Frequency Provided in SAMD21 Datasheet
    error_blink(ERR_SD_INIT);
  // Select Logging File Path
  short id = 0;
  char path[100] = "0.dat";
  for (id = 0; (SD.exists(path) && id < 10); id++)
    path[0] = (char)((short)'0' + id);
  // Overwrite if Max Number of Files Reached
  if (id >= 10)
    sprintf(path, "%s", "10.dat");

  // Initialize DMAC Module and Configure DMA Buffers
  dmac_config();
  // Configure ADC-DMA Channel Descriptor
  dmadesc.BTCTRL.bit.VALID = 0;                                        // Invalidate Descriptor
  dmadesc.BTCNT.reg = sizeof(curbuf->DMA_ADCBuf) / 2;                  // Set Maximum Holding Capacity of Buffer
  dmadesc.DESCADDR.reg = 0x0;                                          // Set Descriptor as First and Final Descriptor
  dmadesc.SRCADDR.reg = (uint32_t)&ADC->RESULT.reg;                    // Set ADC Result Register as Source
  dmadesc.DSTADDR.reg = (uint32_t)&curbuf->DMA_ADCBuf[ADC_DMA_BUFLEN]; // Initialize Destination Address to Last Address in DMA Buffer
  dmadesc.BTCTRL.bit.VALID = 1;                                        // Validate Descriptor
  // Enable DMAC and ADC-DMA Channel
  DMAC->CTRL.bit.DMAENABLE = 1;                    // Enable DMAC
  DMAC->CHID.bit.ID = DMAC_CHID_ID(ADC_DMA_CH_ID); // Select ADC-DMA Channel
  DMAC->CHCTRLA.bit.ENABLE = 1;                    // Enable ADC-DMA Channel

  // Initialize ADC Module
  adc_config();

  while (digitalRead(N_ENABLE) == HIGH);

  // Enable ADC and Trigger Freerunning Conversion
  ADC->CTRLA.bit.ENABLE = 1;
  ADC->SWTRIG.bit.START = 1;

  // Mark DMA to Buffer as Active
  curbuf->bufstat &= ~BUF_DMA_STATUS_MASK;                 // Clear Previous DMA Status Flag
  curbuf->bufstat |= BUF_DMA_ACTIVE << BUF_DMA_STATUS_POS; // Mark DMA Write to Buffer as Active

  // Open LogFile and Begin Log
  // Abort if File does not Open
  if (!LogFile.open(path, O_RDWR | O_CREAT | O_TRUNC)) {
    error_blink(ERR_SD_FILE);
    // Serial.println("Failed to open log file.");
    currentState = FAILURE;
    // Serial.println("CURRENT MKR STATE: FAILURE");
  } else {
    digitalWrite(ACK, LOW);
    currentState = ACQUIRE;
    // Serial.println("CURRENT MKR STATE: ACQUIRE");
  }

  // Log DAQ Starting Time
  uint32_t start = micros();
  LogFile.write(&start, sizeof(uint32_t));

  // Indicate Start of Logging
  digitalWrite(LED_BUILTIN, HIGH);
  // Serial.println("\nStarting Logging...");
  // Serial.print("Logging to file: ");
  // Serial.println(path);
}

// ####################################################################### DAQ System Loop Function
void loop()
{
  if (currentState == ACQUIRE) {
  // Check if DMAC Handler ISR Aborted
    if (sdbuf_err)
    {
      LogFile.flush(); // Force any Data in File Buffer to be Written to SD Card
      LogFile.close(); // Close Logging File

      // Indicate SD Write Buffer Error on LED
      error_blink(ERR_SD_BUF);
    }

    // Check if SD Buffer is Ready For SD Write
    if ((sdbuf->bufstat & BUF_DMA_STATUS_MASK) == (BUF_DMA_DONE << BUF_DMA_STATUS_POS))
    {
      // Add Buffer Timestamp
      sdbuf->timestamp = micros();

      // Mark SD Write from Buffer as Active
      sdbuf->bufstat &= ~BUF_SD_STATUS_MASK;                 // Clear Previous SD Write Status Flag
      sdbuf->bufstat |= BUF_SD_WRITING << BUF_SD_STATUS_POS; // Set SD Write Status as Active

      // Dump Buffer to SD Card
      LogFile.write(sdbuf, sizeof(DataBuf));

      // Check if Finish Signal was Received from DMAC Handler ISR
      if (finished)
      {
        LogFile.flush(); // Force any Data in File Buffer to be Written to SD Card
        LogFile.close(); // Close Logging File
        currentState = FINISH;

        // Clear All Buffers
        memset(curbuf, 0X00, sizeof(DataBuf));
        memset(sdbuf, 0X00, sizeof(DataBuf));

        // Indicated Logging Finished Status on LED
        error_blink(ERR_LOG_FINISHED);
      }
      else
      {
        // Clear DMA Buffer
        memset(sdbuf->DMA_ADCBuf, 0X00, sizeof(sdbuf->DMA_ADCBuf));

        // Mark SD Write from Buffer as Complete
        sdbuf->bufstat &= ~BUF_SD_STATUS_MASK;                 // Clear Previous SD Write Status Flag
        sdbuf->bufstat |= BUF_SD_WRITTEN << BUF_SD_STATUS_POS; // Set SD Write Status as Completed

        // Mark Buffer Ready For DMA
        sdbuf->bufstat &= ~BUF_DMA_STATUS_MASK;                   // Clear Previous DMA Status Flag
        sdbuf->bufstat |= BUF_DMA_INACTIVE << BUF_DMA_STATUS_POS; // Mark Buffer Ready for DMA
      }
    }
  } else if (currentState == FINISH) {
    // Serial.println("Logging stopped.");
    digitalWrite(ACK, HIGH);
    while(1);
  }
}
