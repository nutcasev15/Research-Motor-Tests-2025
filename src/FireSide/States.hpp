#ifndef _STATES_H_
#define _STATES_H_
// #### Library Headers
// Finite State Machine State ID Definitions
#include <FiniteState.h>


// #### State Definitions
// Possible States of FSM
enum States : id_t
{
  BOOT,     // Initial Boot State
  SAFE,     // SD Ready
  ARM,      // DMA and ADC Ready
  LAUNCH,   // Igniters Fired
  LOGGING,  // DMA Active
  CONVERT,  // CSV File Generation
  FAILURE   // Diagnostic Mode
};


// #### State Machine Predicates
// Check if Boot can Proceed to SAFE
bool BootCheck(id_t state);

// Check if System can Proceed to ARM
bool SafeCheck(id_t state);

// Check if ARM can Proceed to LAUNCH
bool ArmCheck(id_t state);

// Check if LAUNCH can Proceed to LOGGING
bool LaunchCheck(id_t state);

// Check if LOGGING is Complete
bool LoggingCheck(id_t state);

// Check if CSV Conversion is Complete
bool ConvertCheck(id_t state);

// Check why System is in a Failure State
bool FailureCheck(id_t state);


// #### State Machine Transition Processes
// Handle BOOT > SAFE
void BootSafeTransition();

// Handle BOOT > CONVERT
void BootConvertTransition();

// Handle SAFE > ARM
void SafeArmTransition();

// Handle Arming Failure
void ArmFailureTransition();

// Handle Launch Command
void ArmLaunchTransition();

// Handle Stop of Binary Logging
void LoggingConvertTransition();

// Handle CONVERT > SAFE
void ConvertSafeTransition();

#endif
