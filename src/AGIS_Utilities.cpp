#include <AGIS_Types.h>

const char *getInfusionState(infusionState_t state) {
  switch (state) {
  case infusionState_t::NOT_STARTED:
    return "NOT_STARTED";
  case infusionState_t::STARTED:
    return "STARTED";
  case infusionState_t::IN_PROGRESS:
    return "IN_PROGRESS";
  case infusionState_t::ALARM_COMPLETED:
    return "ALARM_COMPLETED";
  case infusionState_t::ALARM_STOPPED:
    return "ALARM_STOPPED";
  default:
    return "Undefined infusion state";
    break;
  }
}

const char *getMotorState(motorState_t state) {
  switch (state) {
  case motorState_t::UP:
    return "UP";
  case motorState_t::DOWN:
    return "DOWN";
  case motorState_t::OFF:
    return "OFF";
  default:
    return "Undefined motor state";
    break;
  }
}

const char *getButtonState(buttonState_t state) {
  switch (state) {
  case buttonState_t::UP:
    return "UP";
  case buttonState_t::DOWN:
    return "DOWN";
  case buttonState_t::ENTER:
    return "ENTER";
  case buttonState_t::IDLE:
    return "IDLE";
  default:
    return "Undefined motor state";
    break;
  }
}