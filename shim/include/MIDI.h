// Arduino MIDI Library (FortySevenEffects) stub. Incoming messages come from
// control-script lines such as `at 1.0 noteon 60 100`; outgoing messages are
// logged to <logs>.pins.csv as midi_out events.
#pragma once
#include "Arduino.h"
#include "sim_api.h"

#define MIDI_CHANNEL_OMNI 0
#define MIDI_CHANNEL_OFF 17

namespace midi {
enum MidiType : uint8_t {
  InvalidType = 0x00, NoteOff = 0x80, NoteOn = 0x90, AfterTouchPoly = 0xA0, ControlChange = 0xB0,
  ProgramChange = 0xC0, AfterTouchChannel = 0xD0, PitchBend = 0xE0, SystemExclusive = 0xF0,
  Clock = 0xF8, Start = 0xFA, Continue = 0xFB, Stop = 0xFC
};
typedef uint8_t Channel;
typedef uint8_t DataByte;

class MidiInterface {
public:
  void begin(Channel ch = 1) { listen_ = ch; }
  bool read() { return read(listen_); }
  bool read(Channel ch) {
    uint8_t st, d1, d2;
    if (!rig_midi_pop(st, d1, d2)) return false;
    type_ = (MidiType)(st & 0xF0);
    chan_ = (uint8_t)((st & 0x0F) + 1);
    d1_ = d1; d2_ = d2;
    if (ch != MIDI_CHANNEL_OMNI && ch != chan_) return false;
    if (type_ == NoteOn && d2 == 0) type_ = NoteOff;
    switch (type_) {
      case NoteOn: if (on_) on_(chan_, d1, d2); break;
      case NoteOff: if (off_) off_(chan_, d1, d2); break;
      case ControlChange: if (cc_) cc_(chan_, d1, d2); break;
      case ProgramChange: if (pc_) pc_(chan_, d1); break;
      case PitchBend: if (pb_) pb_(chan_, (int)((d2 << 7) | d1) - 8192); break;
      default: break;
    }
    return true;
  }
  MidiType getType() const { return type_; }
  Channel getChannel() const { return chan_; }
  DataByte getData1() const { return d1_; }
  DataByte getData2() const { return d2_; }
  void setHandleNoteOn(void (*f)(Channel, byte, byte)) { on_ = f; }
  void setHandleNoteOff(void (*f)(Channel, byte, byte)) { off_ = f; }
  void setHandleControlChange(void (*f)(Channel, byte, byte)) { cc_ = f; }
  void setHandleProgramChange(void (*f)(Channel, byte)) { pc_ = f; }
  void setHandlePitchBend(void (*f)(Channel, int)) { pb_ = f; }
  void turnThruOff() {}
  void turnThruOn() {}
  void sendNoteOn(byte n, byte v, Channel c) { rig_midi_out((uint8_t)(0x90 | ((c - 1) & 15)), n, v); }
  void sendNoteOff(byte n, byte v, Channel c) { rig_midi_out((uint8_t)(0x80 | ((c - 1) & 15)), n, v); }
  void sendControlChange(byte n, byte v, Channel c) { rig_midi_out((uint8_t)(0xB0 | ((c - 1) & 15)), n, v); }
  void sendProgramChange(byte n, Channel c) { rig_midi_out((uint8_t)(0xC0 | ((c - 1) & 15)), n, 0); }
  void sendPitchBend(int v, Channel c) {
    int u = v + 8192;
    rig_midi_out((uint8_t)(0xE0 | ((c - 1) & 15)), (uint8_t)(u & 127), (uint8_t)((u >> 7) & 127));
  }

private:
  Channel listen_ = 1, chan_ = 1;
  MidiType type_ = InvalidType;
  DataByte d1_ = 0, d2_ = 0;
  void (*on_)(Channel, byte, byte) = nullptr;
  void (*off_)(Channel, byte, byte) = nullptr;
  void (*cc_)(Channel, byte, byte) = nullptr;
  void (*pc_)(Channel, byte) = nullptr;
  void (*pb_)(Channel, int) = nullptr;
};
}  // namespace midi

#define MIDI_CREATE_INSTANCE(Type, SerialPort, Name) midi::MidiInterface Name;
#define MIDI_CREATE_DEFAULT_INSTANCE() midi::MidiInterface MIDI;
