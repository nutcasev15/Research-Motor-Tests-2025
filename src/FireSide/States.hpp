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
bool BootProceedCheck(id_t state);

// Check if Boot Should Redirect to CONVERT
bool BootRedirectCheck(id_t state);

// Check if System can Proceed to ARM
bool SafeCheck(id_t state);

// Check if ARM can Proceed to LAUNCH
bool ArmProceedCheck(id_t state);

// Check if ARM Should Redirect to SAFE
bool ArmRedirectCheck(id_t state);

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
void BootSafeProcess(id_t state);

// Handle BOOT > CONVERT
void BootConvertProcess(id_t state);

// Handle SAFE > ARM
void SafeArmProcess(id_t state);

// Handle Arming Failure
void ArmFailureProcess(id_t state);

// Handle Launch Command
void ArmLaunchProcess(id_t state);

// Handle Logging Indication After Firing Igniters
void LaunchLoggingProcess(id_t state);

// Handle Stop of Binary Logging
void LoggingConvertProcess(id_t state);

// Handle CONVERT > SAFE
void ConvertSafeProcess(id_t state);

// Handle Safe State Transition After Failure
void FailureSafeProcess(id_t state);

#endif
