#ifndef _INTERFACES_H_
#define _INTERFACES_H_
// #### Library Headers
// Arduino Framework and Data Types
#include <Arduino.h>

// STM32 L4 Board HAL Include
#include <stm32l4xx_hal.h>
// Disable Arduino Framework Access to ADC
#define HAL_ADC_MODULE_ONLY

// SPI Hardware Driver for SD Card IO
#include <SPI.h>

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


// Setup Serial Communication Interface
// RYLR998 Hardware Interface
// See REYAX RYLR998 Datasheet for UART Configuration
#define RYLR_UART_BAUD 115200UL

// #define USE_USB_SERIAL
#ifdef USE_USB_SERIAL
// Redirect RYLR to USB Communications and STLink Serial
#define RYLR Serial
#else
// Link Hardware Serial (Pins D0 & D1) to RYLR
// See CN4 on Page 29 of MB1180 Nucleo L412KB Board User Manual
#define RYLR_UART_TX PA9
#define RYLR_UART_RX PA10
inline HardwareSerial RYLR(RYLR_UART_RX, RYLR_UART_TX);
#endif

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

  // Issue Payload Length Including Header
  RYLR.print(Data.length() + 4);

  // Issue FireSide PCB Header
  RYLR.print(",FS> ");

  // Issue Data and Complete Command with Line End
  // CRLF Line End is Mandatory
  RYLR.print(Data);
  RYLR.print("\r\n");

  return;
}


// Number of Concurrently Logged ADC Channels
// 1 Channel Corresponds to A0 on Pinout
// 4 Channels Correspond to A0 to A3 on Pinout
// 5 Channels Correspond to A0 to A6 on Pinout
#define ADC_PARALLEL_CHANNELS 6

// See CN4 on Page 29 of MB1180 Nucleo L412KB Board User Manual
// A7 Excluded to Avoid Conflict with Virtual COM Peripheral
#define MAX_PARALLEL_CHANNELS 7
#if ADC_PARALLEL_CHANNELS < 1 || ADC_PARALLEL_CHANNELS > MAX_PARALLEL_CHANNELS
#error "Too Few or Too Many ADC Channels Configured for Logging"
#error "Minimum Configured Channels = 1"
#error "Maximum Configured Channels = 7"
#endif

// ADC Pins, Channel and Rank Configuration Structure
// Wrap HAL Defines into Arrays for Pin and Channel Configuration
// See CN4 on Page 29 of MB1180 Nucleo L412KB Board User Manual
struct ADCHardwareConfig {
  uint8_t pin;
  uint32_t pad;
  uint32_t channel;
  uint32_t rank;
};

const ADCHardwareConfig ADCHardwareSetup[] = {
  {PIN_A0, PA0, ADC_CHANNEL_5, ADC_REGULAR_RANK_1},
  {PIN_A1, PA1, ADC_CHANNEL_6, ADC_REGULAR_RANK_2},
  {PIN_A2, PA3, ADC_CHANNEL_8, ADC_REGULAR_RANK_3},
  {PIN_A3, PA4, ADC_CHANNEL_9, ADC_REGULAR_RANK_4},
  {PIN_A4, PA5, ADC_CHANNEL_10, ADC_REGULAR_RANK_5},
  {PIN_A5, PA6, ADC_CHANNEL_11, ADC_REGULAR_RANK_6},
  {PIN_A6, PA7, ADC_CHANNEL_12, ADC_REGULAR_RANK_7}
};


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
inline void ErrorBlink(uint8_t CODE)
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
