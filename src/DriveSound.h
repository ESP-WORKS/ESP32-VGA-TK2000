#pragma once
#include <stdint.h>

// Low-cost mechanical approximation of the Apple Disk II for the single
// GPIO25/LEDC PWM output. Unlike the earlier random-pitch beeps, this model
// uses short, decaying stepper impacts and quieter, irregular motor/flux
// texture. There is NO I/O, heap allocation, PCM DMA, or interference with
// the real Disk II controller. It cannot recreate a real sampled drive on a
// single PWM carrier; the guest's C030 speaker always takes priority.
struct TKDriveSoundState {
  uint32_t reads;
  uint32_t writes;
  uint32_t steps;
  bool motor;
  TKDriveSoundState() : reads(0), writes(0), steps(0), motor(false) {}
};

class TKDriveSound {
public:
  enum Effect : uint8_t { SILENT, SPINDLE, READ, WRITE, SEEK };
  struct Frame {
    uint16_t hz;
    uint8_t duty;
    Effect effect;
    uint8_t drive;
    Frame(uint16_t f=0, uint8_t d=0, Effect e=SILENT, uint8_t driveIndex=255)
      : hz(f), duty(d), effect(e), drive(driveIndex) {}
  };

  void reset() {
    initialized_=false;
    rand_=0xA36532D5u;
    lastFrameAt_=0;
    lastFrame_=Frame();
    for (int d=0;d<2;++d) {
      lastRead_[d]=lastWrite_[d]=lastStep_[d]=0;
      readAt_[d]=writeAt_[d]=seekAt_[d]=motorAt_[d]=0;
      seekUntil_[d]=0;
      seekSteps_[d]=0;
      readSeen_[d]=writeSeen_[d]=false;
      lastMotor_[d]=false;
    }
  }

  void arm(const TKDriveSoundState (&drive)[2]) {
    reset();
    initialized_=true;
    for (int d=0;d<2;++d) {
      lastRead_[d]=drive[d].reads;
      lastWrite_[d]=drive[d].writes;
      lastStep_[d]=drive[d].steps;
      lastMotor_[d]=drive[d].motor;
    }
  }

  Frame sample(uint32_t nowMs, const TKDriveSoundState (&drive)[2], bool enabled) {
    if (!enabled) { reset(); return Frame(); }
    if (!initialized_) arm(drive);
    for (int d=0;d<2;++d) {
      if (drive[d].reads!=lastRead_[d]) {
        lastRead_[d]=drive[d].reads;
        readAt_[d]=nowMs;
        readSeen_[d]=true;
      }
      if (drive[d].writes!=lastWrite_[d]) {
        lastWrite_[d]=drive[d].writes;
        writeAt_[d]=nowMs;
        writeSeen_[d]=true;
      }
      const uint32_t steps=drive[d].steps-lastStep_[d];
      if (steps) {
        lastStep_[d]=drive[d].steps;
        seekAt_[d]=nowMs;
        // A burst is multiple separated impacts, not one long high beep.
        seekSteps_[d]=(uint8_t)(steps>8u?8u:steps);
        seekUntil_[d]=nowMs+uint32_t(seekSteps_[d])*13u+24u;
      }
      if (drive[d].motor && !lastMotor_[d]) motorAt_[d]=nowMs;
      lastMotor_[d]=drive[d].motor;
    }

    int which=-1;
    Effect effect=SILENT;
    for (int d=0;d<2;++d) {
      const bool seeking=(int32_t)(seekUntil_[d]-nowMs)>0;
      if (seeking) { which=d; effect=SEEK; break; }
      if (!drive[d].motor) continue;
      const bool writing=writeSeen_[d] && (uint32_t)(nowMs-writeAt_[d])<95u;
      const bool reading=readSeen_[d] && (uint32_t)(nowMs-readAt_[d])<55u;
      const Effect current=writing?WRITE:(reading?READ:SPINDLE);
      if (which<0 || current>effect) { which=d; effect=current; }
    }
    if (which<0) { lastFrame_=Frame(); return lastFrame_; }

    // Changes every ~2ms at most: smooth enough for click envelopes without
    // reprogramming the ESP32 LEDC frequency on each emulated nibble.
    if (lastFrame_.effect==effect && lastFrame_.drive==(uint8_t)which &&
        (uint32_t)(nowMs-lastFrameAt_)<2u) return lastFrame_;
    lastFrameAt_=nowMs;
    rand_^=rand_<<13;
    rand_^=rand_>>17;
    rand_^=rand_<<5;
    const uint16_t noise=(uint16_t)rand_;
    const uint16_t variation=(uint16_t)(which*5u);
    const uint32_t activityAge=(uint32_t)(nowMs-(effect==WRITE?writeAt_[which]:readAt_[which]));

    switch (effect) {
      case SEEK: {
        const uint32_t age=(uint32_t)(nowMs-seekAt_[which]);
        const uint32_t groupMs=uint32_t(seekSteps_[which])*13u;
        if (age<groupMs) {
          const uint8_t t=(uint8_t)(age%13u);
          // Hard metallic attack, damped body and brief rattling tail.
          if (t<3u)
            lastFrame_=Frame((uint16_t)(1710u+variation*8u+(noise%260u)),
                             (uint8_t)(70u+(noise&15u)),SEEK,(uint8_t)which);
          else if (t<7u)
            lastFrame_=Frame((uint16_t)(440u+variation*4u+(noise%135u)),
                             (uint8_t)(32u+(noise&15u)),SEEK,(uint8_t)which);
          else
            lastFrame_=Frame((uint16_t)(205u+variation+(noise%65u)),
                             (uint8_t)(8u+(noise&9u)),SEEK,(uint8_t)which);
        } else {
          const uint32_t tailAge=age-groupMs;
          const uint8_t envelope=(tailAge<24u)?(uint8_t)(24u-tailAge):0u;
          lastFrame_=Frame((uint16_t)(110u+variation+(noise%45u)),
                           (uint8_t)(envelope/5u+1u),SEEK,(uint8_t)which);
        }
        break;
      }
      case WRITE: {
        // Media flux is mostly inaudible; the real character comes from
        // spindle/stepper. Keep the write scrape subdued and irregular.
        const uint8_t grain=(uint8_t)((noise>>4u)&15u);
        const uint8_t envelope=(activityAge<35u)?5u:0u;
        lastFrame_=Frame((uint16_t)(315u+variation+(noise%95u)),
                         (uint8_t)(8u+grain+envelope),WRITE,(uint8_t)which);
        break;
      }
      case READ: {
        const uint8_t grain=(uint8_t)((noise>>5u)&11u);
        lastFrame_=Frame((uint16_t)(170u+variation+(noise%58u)),
                         (uint8_t)(5u+grain),READ,(uint8_t)which);
        break;
      }
      case SPINDLE: {
        const uint32_t motorAge=(uint32_t)(nowMs-motorAt_[which]);
        // Low, slightly uneven bearing rumble. Small initial spin-up bump.
        const uint8_t bump=(motorAge<140u)?4u:0u;
        const uint8_t flutter=(uint8_t)(((nowMs/11u+which*3u)%7u)==0u?3u:0u);
        lastFrame_=Frame((uint16_t)(82u+variation+(noise%11u)),
                         (uint8_t)(4u+bump+flutter+(noise&3u)),SPINDLE,(uint8_t)which);
        break;
      }
      default: lastFrame_=Frame(); break;
    }
    return lastFrame_;
  }
private:
  bool initialized_=false;
  uint32_t rand_=0xA36532D5u;
  uint32_t lastRead_[2]={},lastWrite_[2]={},lastStep_[2]={};
  uint32_t readAt_[2]={},writeAt_[2]={},seekAt_[2]={},motorAt_[2]={},seekUntil_[2]={};
  uint8_t seekSteps_[2]={};
  bool readSeen_[2]={},writeSeen_[2]={},lastMotor_[2]={};
  uint32_t lastFrameAt_=0;
  Frame lastFrame_;
};
