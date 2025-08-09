#ifndef _DMADAQ_H_
#define _DMADAQ_H_
// #### Library Headers
// STM32 L4 Board HAL Include
#include <stm32l4xx_hal.h>
// Disable Arduino Framework Access to ADC
#define HAL_ADC_MODULE_ONLY


// #### DMA Data Logging Functions
// DMA Module Configuration
void ConfigureDMA(bool Continuous = false);

// Successful Block One DMA Transfer Completion Callback
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc);

// Successful Block Two DMA Transfer Completion Callback
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);

// ADC Module Configuration
void ConfigureADC(bool Continuous = false);

// Readout Analog Pins to Check Input
void ReadoutAnalogPins();

// Binary Log File Name Helper
String GetLogfileName(bool Initialise = true);

// Binary Log File and Initial DMA Buffer Configuration
void ConfigureLogging();

// Coupled ADC-DMA Transfer and Logging Trigger
void TriggerLogging();

// Log Finalised Binary DMA Buffers to SD Card
bool LogBuffers();

// Binary Log File to CSV File Converter
void ConvertLog(const String &Path);

#endif
