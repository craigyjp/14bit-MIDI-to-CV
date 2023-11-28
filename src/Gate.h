#ifndef Gate_h
#define Gate_h

#include <Arduino.h>

class Gate {
public:
  Gate(uint8_t index);
  void noteOn(uint8_t velocity);
  void noteOff(uint8_t velocity);
  bool tick();

private:
  unsigned long _halt_at;
  bool _on;
  uint8_t _velocity;
};

#endif