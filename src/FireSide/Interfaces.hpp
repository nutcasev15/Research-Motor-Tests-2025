#ifndef _INTERFACES_H_
#define _INTERFACES_H_
// #### Library Headers
// Arduino Framework and Data Types
#include <Arduino.h>

// STM32 L4 Board HAL Include
#include <stm32l4xx_hal.h>
// Disable Arduino Framework Access to ADC
#define HAL_ADC_MODULE_ONLY

// File Class for Binary Data Container
#include <SD.h>


// #### HW Configuration Declarations
// Status Pin for Visual Output
#define STATUS_PIN 2

// MOSFET Latch Pins for Firing Igniters
#define FIRE_PIN_A 3
#define FIRE_PIN_B 6
#define FIRE_PIN_C 9
// D4184 Input Firing Logic
#define STATUS_FIRE HIGH
#define STATUS_SAFE LOW


// RYLR998 Hardware Interface
// Alias Hardware Serial (Pins D0 & D1) to RYLR
// See REYAX RYLR998 Datasheet for UART Configuration
#define RYLR_UART_BAUD 115200UL
#define RYLR_UART_TX PIN_SERIAL_TX
#define RYLR_UART_RX PIN_SERIAL_RX
#define RYLR Serial

// Parse Incoming GroundSide Commands via RYLR Module
inline void ParseRYLR(String &Buffer)
{
  if (!RYLR.available())
  {
    // Return Blank
    Buffer = '\n';
    return;
  }

  // Load Incoming Data
  String parsed = RYLR.readStringUntil('\n');

  // See +RCV in REYAX AT RYLRX98 Commanding Datasheet
  // Remove Data from Last 2 Fields
  parsed.remove(parsed.lastIndexOf(','));
  parsed.remove(parsed.lastIndexOf(','));

  // Extract Data in 3rd Comma Separated Field
  Buffer = parsed.substring(parsed.lastIndexOf(',') + 1);

  // Strip Carriage Return
  Buffer.trim();

  return;
}

// Send Data to GroundSide via RYLR Module
inline void SendRYLR(const String &Data)
{
  // Issue Send AT Command
  // See +SEND in REYAX AT RYLRX98 Commanding Datasheet
  RYLR.print("AT+SEND=0,");

  // Issue Payload Length Including Line End
  RYLR.print(Data.length() + 5);

  // Issue FireSide PCB Header
  RYLR.print(",FS> ");

  // Issue Data and Complete Command with Line End
  RYLR.print(Data);
  RYLR.print('\n');

  return;
}


// Number of Concurrently Logged ADC Channels
// 1 Channel Corresponds to A7 on Pinout
// 4 Channels Correspond to A7 and A2 to A4 on Pinout
// 6 Channels Correspond to A7 and A2 to A6 on Pinout
#define ADC_PARALLEL_CHANNELS 6
#if ADC_PARALLEL_CHANNELS < 1 || ADC_PARALLEL_CHANNELS > 6
#error "Too Few or Too Many ADC Channels Configured for Logging"
#error "Minimum Configured Channels = 1"
#error "Maximum Configured Channels = 6"
#endif

// ADC Channel and Rank Configuration Helper Arrays
// Wrap HAL Defines into Arrays for Channel Configuration
static uint32_t ADCInputChannels[] = {
  ADC_CHANNEL_7,
  ADC_CHANNEL_8,
  ADC_CHANNEL_9,
  ADC_CHANNEL_10,
  ADC_CHANNEL_11,
  ADC_CHANNEL_12
};

static uint32_t ADCRegularRanks[] = {
  ADC_REGULAR_RANK_1,
  ADC_REGULAR_RANK_2,
  ADC_REGULAR_RANK_3,
  ADC_REGULAR_RANK_4,
  ADC_REGULAR_RANK_5,
  ADC_REGULAR_RANK_6
};


// Boolean to Track SD Write State
volatile static bool SDWriting;

// Boolean to Track Finish Signal
volatile static bool finished;

// ADC and DMA Interface using STM32 HAL
static ADC_HandleTypeDef hadc1;
static DMA_HandleTypeDef hdma_adc1;

// Binary File Object and Name on SD Card
static File LogFile;
static String FileName;


// #### Error Definitions
// Throw this if ADC HAL Initialisation Fails
#define ERR_HAL_ADC 1
// Throw this if DMA HAL Initialisation Fails
#define ERR_HAL_DMA 2
// Throw this if SD Card Initialisation Fails
#define ERR_SD_INIT 3
// Throw this if File is not Available
#define ERR_SD_FILE 4
// Throw this if SD Data Buffer is not Free
#define ERR_SD_BUFF 5

// Indicate Error on Status Pin
static inline void ErrorBlink(uint8_t CODE)
{
  // Set Blink Repeating Interval (ms)
  const uint16_t period = 5000;

  // Assemble and Transmit Status over RYLR
  String status = "Error Code: ";
  status += CODE;
  SendRYLR(status);

  // Turn Indicator LED Off
  digitalWrite(STATUS_PIN, LOW);
  // Blink CODE Number of Times, Then Wait and Repeat
  while (1)
  {
    for (short i = 0; i < CODE; i++)
    {
      digitalWrite(STATUS_PIN, HIGH);
      delay(period / (2 * CODE));
      digitalWrite(STATUS_PIN, LOW);
      delay(period / (2 * CODE));
    }
    delay(period / 2);
  }
}

#endif
