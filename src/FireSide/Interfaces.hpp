#ifndef _INTERFACES_H_
#define _INTERFACES_H_
// #### Library Headers
// Arduino Framework and Data Types
#include <Arduino.h>

// Adafruit ZeroDMA Class and Descriptors
#include <Adafruit_ZeroDMA.h>

// File Class for Binary Data Container
#include <SD.h>


// #### HW Configuration Declarations
// MOSFET Latch Pins for Firing Igniters
#define FIRE_PIN_A 3
#define FIRE_PIN_B 4
// Firing Circuit uses Inverted Logic
#define STATUS_FIRE LOW
#define STATUS_SAFE HIGH

// Status Pin for Visual Output
#define STATUS_PIN 5


// RYLR998 Hardware Interface
// Alias Hardware Serial1 (Pins 13 & 14) to RYLR
// See REYAX RYLR998 Datasheet for UART Configuration
#define RYLR_UART_BAUD 115200UL
#define RYLR_UART_TX PIN_SERIAL1_TX
#define RYLR_UART_RX PIN_SERIAL1_RX
#define RYLR Serial1

// Parse Incoming GroundSide Commands via RYLR Module
static inline void ParseRYLR(String &Buffer)
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

// Send Data to GroundSide via RYLR Module
static inline void SendRYLR(const String &Data)
{
  // Issue Send AT Command
  // See +SEND in REYAX AT RYLRX98 Commanding Datasheet
  RYLR.print("AT+SEND=0,");

  // Issue Payload Length
  RYLR.print(Data.length() + 4);

  // Issue FireSide PCB Header
  RYLR.print(",FS> ");

  // Issue Data and Complete Command with Line End
  RYLR.println(Data);
}


// Number of Concurrently Logged ADC Channels
// 1 Channel Corresponds to A3 on Pinout
// 4 Channels Correspond to A3 to A6 on Pinout
// 8 Channels Correspond to A3 to A6, 2 Junk Channels and A1 to A2
#define ADC_PARALLEL_CHANNELS 8
#if ADC_PARALLEL_CHANNELS < 1 || ADC_PARALLEL_CHANNELS > 8
#error "Too Few or Too Many ADC Channels Configured for Logging"
#error "Minimum Configured Channels = 1"
#error "Maximum Configured Channels = 8"
#endif


// Boolean to Track Finish Signal
volatile static bool finished;

// DMA Interface using Zero DMA Library
static Adafruit_ZeroDMA DMA;

// Binary File Object and Name on SD Card
static File LogFile;
static String FileName;


// #### Error Definitions
// Throw this if SD Card Initialization Fails
#define ERR_SD_INIT 1
// Throw this if File is not Available
#define ERR_SD_FILE 2
// Throw this if SD Data Buffer is not Free
#define ERR_SD_BUFF 3

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
