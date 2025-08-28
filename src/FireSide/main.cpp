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
Transition StateTransitions[] =
{
  {    BootCheck, CONVERT,    SAFE},
  {    SafeCheck,    SAFE,     ARM},
  {     ArmCheck, FAILURE,  LAUNCH},
  {  LaunchCheck,  LAUNCH, LOGGING},
  { LoggingCheck, LOGGING, CONVERT},
  { ConvertCheck, CONVERT,    SAFE},
  { FailureCheck, FAILURE,    SAFE}
};

// Calculate Total Number of Transitions
const uint8_t TotalTransitions = sizeof(StateTransitions) / sizeof(Transition);

// Initialize Finite State Machine with Defined Transitions
FiniteState FSM(StateTransitions, TotalTransitions);


// #### STM32 Nucleo L412KB Hardware Setup
void setup()
{
  // Setup Igniters Pins
  pinMode(FIRE_PIN_A, OUTPUT);
  pinMode(FIRE_PIN_B, OUTPUT);
  pinMode(FIRE_PIN_C, OUTPUT);
  // Ensure Igniter MOSFETS are Off
  digitalWrite(FIRE_PIN_A, STATUS_SAFE);
  digitalWrite(FIRE_PIN_B, STATUS_SAFE);
  digitalWrite(FIRE_PIN_C, STATUS_SAFE);

  // Setup Status Pin and Initialise Indicator
  pinMode(STATUS_PIN, OUTPUT);
  digitalWrite(STATUS_PIN, LOW);

  // Begin Finite State Machine in BOOT State
  FSM.begin(BOOT);
}


// #### STM32 Nucleo L412KB Operations
void loop()
{
  // Evaluate Predicate of Current State
  // Transition if Predicate Check Returns True
  FSM.execute();
}
