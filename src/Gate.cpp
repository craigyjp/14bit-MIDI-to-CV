#include "Gate.h"

Gate::Gate(uint8_t index) {
  _halt_at = 0;
  _on = false;
  _velocity = 0;
}

void Gate::noteOn(byte velocity) {
  _halt_at = millis() + 2;
  _on = true;
  _velocity = velocity;
}

void Gate::noteOff(byte velocity) {
  _halt_at = 0;
  _on = false;
  _velocity = velocity;
}

bool Gate::tick() {
  if (_on && millis() > _halt_at) {
    noteOff(0);
    return true;
  }
  return false;
}