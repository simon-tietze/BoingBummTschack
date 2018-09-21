#ifndef Pads_h
#define Pads_h

#include <Arduino.h>

typedef struct {
  byte n;
  float data[4];
  byte last_pos;
} CircleBuffer;

typedef struct {
  char physical_pin;
  byte midi_note;
  float gain;
  float min_velocity;
  byte xtalk_group;
  unsigned long mask_micros;
  CircleBuffer values;
  CircleBuffer values_lowp;
  float last_peak;
  unsigned long last_trigger_time;
  float last_trigger_value;
  bool active;
} Pad;

void alloc_buffer(CircleBuffer *b, byte n);
float current(CircleBuffer *b);
float prev(CircleBuffer *b, int lag);
void record_value(CircleBuffer *b, float v);
void read_pin(Pad *p);
void update_lowp(Pad *p);
float peakify(Pad *p);

#endif

