#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Cassette.h"
#include "TKTapePCM97.h"
#include "TKPCMDriveAudio.h"
#include "TKMainMenu97.h"

static unsigned crossings(TKTapePCM97& wave,unsigned samples) {
  unsigned changes=0;
  int previous=wave.nextSample();
  for(unsigned i=1;i<samples;++i) {
    const int value=wave.nextSample();
    if((value>0)!=(previous>0))++changes;
    previous=value;
  }
  return changes;
}

int main() {
  // Java Tape.read() uses C030 toggles at real tape wave half-cycles;
  // at 16000 Hz we must hear different bit-0/bit-1 speeds, not fixed hum.
  TKTapePCM97 wave;
  wave.setEnabled(true);
  wave.signal(true,0x80,500);
  const unsigned slow=crossings(wave,320);
  wave.stop();
  wave.signal(true,0x80,250);
  const unsigned fast=crossings(wave,320);
  assert(slow>20 && slow<50);
  assert(fast>slow*16/10 && fast<100);
  wave.setEnabled(false);
  for(int i=0;i<64;++i)assert(wave.nextSample()==0);
  wave.setEnabled(true);
  assert(wave.nextSample()!=0);
  wave.setPaused(true);
  for(int i=0;i<64;++i)assert(wave.nextSample()==0);
  wave.setPaused(false);
  assert(wave.nextSample()!=0);
  wave.stop();
  assert(wave.nextSample()==0);
  // A fresh cassette normally starts with the same leader half-period.
  wave.signal(true,0,500);
  assert(wave.nextSample()!=0);
  wave.signal(true,0x80,500);
  assert(wave.nextSample()>0); // phase aligns to a real CASIN transition
  wave.signal(true,0,500);
  assert(wave.nextSample()<0);
  wave.signal(true,0x80,25000); // reject a noisy/stalled old record edge
  assert(wave.nextSample()==0);

  TKPCMDriveAudio mixer;
  mixer.setDriveEnabled(false);
  mixer.setTapeSoundEnabled(false);
  mixer.updateTapeSignal(true,0x80,500);
  for(int i=0;i<128;++i)assert(mixer.nextSample()==0);
  mixer.setTapeSoundEnabled(true);
  assert(mixer.nextSample()!=0); // audible even when drives are OFF
  mixer.setPaused(true);
  for(int i=0;i<32;++i)assert(mixer.nextSample()==0);
  mixer.setPaused(false);
  assert(mixer.nextSample()!=0);
  mixer.stopTapeSignal();
  assert(mixer.nextSample()==0);
  mixer.setSpeakerHz(440);
  bool heardSpeaker=false;
  for(int i=0;i<150;++i)if(mixer.nextSample()!=0)heardSpeaker=true;
  assert(heardSpeaker); // K7 mute never disables the original C030 speaker

  // Synthetic CTK2 tape: first bit=1 (500/500), next bit=0 (250/250).
  uint8_t ct2[]={'C','T','K','2','D','A',1,0,0x80};
  uint8_t* owned=(uint8_t*)malloc(sizeof(ct2));
  assert(owned);memcpy(owned,ct2,sizeof(ct2));
  CassetteCT2 tape;
  assert(tape.mount(owned,sizeof(ct2)));
  tape.beginPlay();
  assert(tape.playing() && tape.audioHalfWaveCycles()==500);
  assert(tape.read(100)==0x80);
  assert(tape.read(600)==0x00);
  assert(tape.audioHalfWaveCycles()==500);
  assert(tape.read(1100)==0x80);
  assert(tape.audioHalfWaveCycles()==250);
  tape.togglePause(1200);
  assert(tape.paused() && !tape.playing());
  tape.togglePause(1200);
  assert(tape.playing() && !tape.paused());
  tape.eject();
  assert(!tape.playing() && !tape.mounted());

  CassetteCT2 save;
  save.beginRecord();
  assert(save.recording());
  save.outputToggle(1000);
  save.outputToggle(1250);
  save.outputToggle(1750);
  assert(save.capturedEdges()==2u);
  assert(save.capturedDuration(0)==250 && save.capturedDuration(1)==500);
  save.stopRecord();
  assert(!save.recording());

  assert(TKMainMenu97::rows==10);
  assert(TKMainMenu97::y>=0 && TKMainMenu97::y+TKMainMenu97::height<=200);
  assert(TKMainMenu97::x>=0 && TKMainMenu97::x+TKMainMenu97::width<=640);
  assert(TKMainMenu97::rowY(9)+7 < TKMainMenu97::y+TKMainMenu97::separatorOffset);
  assert(TKMainMenu97::footerOffset+7<TKMainMenu97::height);
  printf("PASS v0.97: Java/CT2 halfwave sound, load/save edges, mute/resume, C030 preserved, F1 10 rows (slow=%u fast=%u)\n",slow,fast);
}
