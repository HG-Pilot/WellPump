#pragma once

// Main Pump State Machine Enum
enum PumpState {
  WELL_RECOVERY,
  STARTING_PUMP,
  PUMPING_WATER,
  STOPPING_PUMP,
  ERROR_RED,
  ERROR_PURPLE,
  ERROR_YELLOW,
  HALTED_PUMP
};

// Declare Global State Variables as Extern
extern PumpState currentState;