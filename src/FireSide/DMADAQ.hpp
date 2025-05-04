#ifndef _DMADAQ_H_
#define _DMADAQ_H_
// #### Library Headers
// Adafruit ZeroDMA Class Definition
#include <Adafruit_ZeroDMA.h>


// #### DMA Data Logging Functions
// ADC Module Configuration
void ConfigureADC();

// Adafruit ZeroDMA Configuration
void ConfigureDMA();

// Successful Transfer Handler
void HandleTransferComplete(Adafruit_ZeroDMA *);

// Failed Transfer Handler
void HandleTransferFail(Adafruit_ZeroDMA *);

// Binary Log File and Initial DMA Buffer Configuration
String ConfigureLogging();

// Coupled ADC-DMA Transfer and Logging Trigger
void TriggerLogging();

// Log Finalised Binary DMA Buffers to SD Card
void LogBuffers();

// Binary Log File to CSV File Converter
void ConvertLog(String path);

#endif
