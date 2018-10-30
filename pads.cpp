#include "pads.h"

void alloc_buffer(CircleBuffer *b, byte n) {
 // b->data = new float[4]();
  b->n = n;
}

float current(CircleBuffer *b) {
  return(b->data[b->last_pos]);
}


float prev(CircleBuffer *b, int lag) {
  int idx = b->last_pos - lag;
  if(idx < 0) {
    idx = idx + b->n;
  } 
  return(b->data[idx]);
}

void record_value(CircleBuffer *b, float v) {
  b->last_pos++;
  if(b->last_pos >= b->n) {
    b->last_pos = 0;
  }
  b->data[b->last_pos] = v;
}

void read_pin(Pad *p) {
  analogRead(p->physical_pin);
  analogRead(p->physical_pin);
  record_value(&p->values, analogRead(p->physical_pin) * p->gain);  
}

// currently mean
void update_lowp(Pad *p) {
  float new_v = 0;
  for(int i = 0; i < p->values.n; i++) {
    new_v += p->values.data[i];
  }
  new_v = new_v / p->values.n;
  record_value(&p->values_lowp, new_v);
}

float peakify(Pad *p) {
  if(prev(&p->values_lowp, 1) - prev(&p->values_lowp, 2) > p->minspeed & // fast enough hit or at least intensity was increasing ( == >0) for most, speed used for pedal hat vs soft hat close)
     prev(&p->values_lowp, 1) - current(&p->values_lowp) >= -1 * p->minspeed) { // and not increasing anymore or even going down
    return(prev(&p->values_lowp, 1));
  } else {
    return(0.0);
  }
}


