// #### Library Headers
// Arduino Framework and Data Types
#include <Arduino.h>

// Finite State Machine Class and Transitions
#include <FiniteState.h>


// #### Internal Headers
// Hardware Igniter and Status Pins
#include "Interfaces.hpp"

// State Predicate and Process Definitions
#include "States.hpp"


// #### Internal Definitions
// Define State Transitions and Corresponding Relationships
Transition StateRelationships[] =
{
  { BootProceedCheck,    BOOT,    SAFE,       BootSafeProcess},
  {BootRedirectCheck,    BOOT, CONVERT,    BootConvertProcess},
  {        SafeCheck,    SAFE,     ARM,        SafeArmProcess},
  { ArmRedirectCheck,     ARM, FAILURE,     ArmFailureProcess},
  {  ArmProceedCheck,     ARM,  LAUNCH,      ArmLaunchProcess},
  {      LaunchCheck,  LAUNCH, LOGGING,  LaunchLoggingProcess},
  {     LoggingCheck, LOGGING, CONVERT, LoggingConvertProcess},
  {     ConvertCheck, CONVERT,    SAFE,    ConvertSafeProcess},
  {     FailureCheck, FAILURE,    SAFE,    FailureSafeProcess}
};

// Calculate Total Number of Transitions
const uint8_t TotalTransitions =
  sizeof(StateRelationships) \
  / sizeof(Transition);

// Initialize Finite State Machine with Defined Transitions
FiniteState FSM(StateRelationships, TotalTransitions);


// #### Arduino MKR Zero Hardware Setup
void setup()
{
  // Setup Igniters Pins
  pinMode(FIRE_PIN_A, OUTPUT);
  pinMode(FIRE_PIN_B, OUTPUT);
  // Ensure Igniter MOSFETS are Off
  digitalWrite(FIRE_PIN_A, LOW);
  digitalWrite(FIRE_PIN_B, LOW);

  // Setup Status Pin and Initialise Indicator
  pinMode(STATUS_PIN, OUTPUT);
  digitalWrite(STATUS_PIN, LOW);

  // Begin Finite State Machine in BOOT State
  FSM.begin(BOOT);
}


// #### Arduino MKR Zero Operations
void loop()
{
  // Evaluate Predicate of Current State
  // Transition if Predicate Check Returns True
  FSM.execute();
}
