#include <frequencyToNote.h>
#include <MIDIUSB.h>
#include <pitchToFrequency.h>
#include <pitchToNote.h>

#include "pads.h"

const boolean DEBUG = false;
const byte CHANNEL = 10;                          
int const N_PADS = 4;
int const N_XTALK_GROUPS = 3;
unsigned long const XTALK_MASK_MICROS = 120000;
byte const N_LP_SAMPLES = 4;
unsigned long xtalk_last_hit[N_XTALK_GROUPS] = {0, 0};
const float BASE_SEND_VELOCITY = 32;
const float MAX_SEND_VELOCITY = 127;


Pad pads[N_PADS] = {
  {.physical_pin = 0,.midi_note = 38, .gain = 1.5, .min_velocity = 300.0, .xtalk_group = 0, .mask_micros = 75000}, // Snare (left)
  {.physical_pin = 1,.midi_note = 46, .gain = 2.0, .min_velocity = 150.0, .xtalk_group = 1, .mask_micros = 75000}, // HiHat (right)
  {.physical_pin = 2,.midi_note = 51, .gain = 2.0, .min_velocity = 250.0, .xtalk_group = 1, .mask_micros = 75000}, // Crash (right side)
  {.physical_pin = 3,.midi_note = 36, .gain = 1.0, .min_velocity = 500.0, .xtalk_group = 2, .mask_micros = 250000}, // Bass (mid)
}; 

int const HIHAT_PEDAL_PIN = 4;

byte pad_order[N_PADS];

int sort_by_peak(const void *cmp1, const void *cmp2) {
  byte pad1 = *((byte *)cmp1);
  byte pad2 = *((byte *)cmp2);
  return pads[pad2].last_peak - pads[pad1].last_peak;
}


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  
  // defines for setting and clearing register bits
  #ifndef cbi
  #define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
  #endif
  #ifndef sbi
  #define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
  #endif
  
  // set prescale to 16
  sbi(ADCSRA,ADPS2);
  cbi(ADCSRA,ADPS1);
  cbi(ADCSRA,ADPS0);
  
  Serial.begin(115200);  
  if(DEBUG) {
    Serial.println("t,m,m_lp,m_p,m_v,m_a,m_ttl,l_lp,l_p,l_v,l_a,l_tt,r,r_lp,r_p,r_v,r_a,r_tt,sr,sr_lp,sr_p,sr_v,s_a,s_tt,xtt0,xtt1,order"); 
  }
  
  for(int p=0; p < N_PADS; p++) {
    pad_order[p] = p;
    pads[p].values.n = 4;
    pads[p].values_lowp.n = 3;
    //alloc_buffer(&pads[p].values, N_LP_SAMPLES);
    //alloc_buffer(&pads[p].values_lowp, 3);
  }
}


void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | channel - 1, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel - 1, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}


void loop() {
  unsigned long t = micros();
  int pedal = analogRead(HIHAT_PEDAL_PIN);
  
  if(DEBUG) {
    Serial.print(t);
    Serial.print(","); 
  }

  if(pads[0].active) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }
  
  for(int p=0; p < N_PADS; p++) {
    read_pin(&pads[p]);
    update_lowp(&pads[p]);
    pads[p].last_peak = peakify(&pads[p]);
  }

  // sort pads in descending order to hit strongest peak first
  // in case of xtalk
  qsort(pad_order, N_PADS, sizeof(pad_order[0]), sort_by_peak );
  
  for(int pi=0; pi < N_PADS; pi++) {
    byte p = DEBUG ? pi : pad_order[pi];
    float v = pads[p].last_peak;
    byte group = pads[p].xtalk_group;
    unsigned long lastt = 0;

    if(v > pads[p].min_velocity) {
      // strong hit
      if(!pads[p].active && ((t - xtalk_last_hit[group]) > XTALK_MASK_MICROS)) {
        // .. and not playing already -> note on
        v = BASE_SEND_VELOCITY + (127.0 - BASE_SEND_VELOCITY) * (v - pads[p].min_velocity) / (1024.0 - pads[p].min_velocity);
        v = v > MAX_SEND_VELOCITY ? MAX_SEND_VELOCITY : v; // clamp
        xtalk_last_hit[group] = t;
        pads[p].last_trigger_time = t;
        lastt = t;
        pads[p].last_trigger_value = v;
        pads[p].active = true;
      } else {
        // strong enough but masked by crosstalk or already playing
        v = -64;
      }
    } else if(pads[p].active){
      // signal not strong enough anymore, but note was playing -> possible note off
      if((t - pads[p].last_trigger_time) > pads[p].mask_micros) {
        // already played long enough -> note off
        pads[p].active = false;
        v = 0;
      } else {
        // still in mask time, hold
        v = -1;
      }
    } else {
      // neither strong nor hodling, send -127 for debug
      v = -127;
    }
    
    
    if(DEBUG) {
      Serial.print(current(&pads[p].values));
      Serial.print(",");
      Serial.print(current(&pads[p].values_lowp));
      Serial.print(",");
      Serial.print(peakify(&pads[p]));
      Serial.print(",");
      Serial.print(v);
      Serial.print(",");
      Serial.print(pads[p].active);
      Serial.print(",");
      Serial.print(lastt);
      Serial.print(",");
    } else {
      if(v > 0) {
        //Serial.print(p);
        //Serial.println(" on");
        noteOn(CHANNEL, (pads[p].midi_note == 46) & (pedal > 512) ? 42 : pads[p].midi_note, (byte) v);
      } else if(v == 0) {
        //Serial.print(p);
        //Serial.println(" off");
        noteOff(CHANNEL, pads[p].midi_note, (byte) 0);
      } 
    }
    
  }

  MidiUSB.flush();
  
  if(DEBUG) {
    for(byte group = 0; group < N_XTALK_GROUPS; group++) {
      Serial.print(xtalk_last_hit[group]);
      Serial.print(",");
    }
    Serial.println(pad_order[0]);
  }
}



