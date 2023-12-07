# 14bit-MIDI-to-CV

Teensy PWM based MIDI to CV converter with 20 channels of CV and 8 triggers

This MIDI to CV converter is loosely based on the MidiMuso CV-12, with additional features such as 20 CV outputs and 8 gates, 4 dedicated outputs for PitchBend, Modulation, Channel Aftertouch and Breath Controller. First 16 outputs are shared between the pitch/velocity CV outputs and assignable outputs. When not in note CV mode, all 16 CV outputs can be assigned to CC numbers or NRPN numbers and the voltage output can be asisgned 0-5V or 0-10v. When the Poly CV mode is activated, then for each note in the poly group a pair of outputs such as 1 & 9 for CV1 are assigned to Pitch CV and Velocity CV, pitch is 1V/oct upto 10V and Velocity if 0-5V, gate 1 is also assigned to the same note. Other gates can be assigned note numbers when not in a Poly CV group and used as triggers for drums etc.

All assignments can be stored in memories and recalled from the patch menu and named.

# Features

* Menu driven setup
* Up to 8 note polyphonic MIDI to CV with velocity
* 16 Assignable CV outputs
* 4 Fixed CV outputs on the poly MIDI channel
* NRPN outputs 1024 steps, 0-5V or 0-10V
* CC outputs 128 steps, 0-5V or 0-10V
* 8 Assignable gates
* MIDI channel assignment
* Transpose
* Octave Shift
* Unison Mode with note priority.
* MIDI Clock output with divide by 1, 2, 4 & 8 and reset output on play.
