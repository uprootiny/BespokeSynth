/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/
//
//  BowedString.h
//
//  Physically modeled bowed string instrument with:
//  - Dual-delay waveguide string with stick-slip friction at bow point
//  - Bridge transfer function (biquad highpass)
//  - Body resonator (parallel bandpass mode bank from violin measurements)
//  - Optional sympathetic string coupling
//
//  The friction model uses the hyperbolic approximation:
//    mu(v) = mu_d + (mu_s - mu_d) * v_break / (|v| + v_break)
//  which produces correct Helmholtz motion without discontinuities.
//

#pragma once

#include "IAudioProcessor.h"
#include "INoteReceiver.h"
#include "IDrawableModule.h"
#include "Slider.h"
#include "ChannelBuffer.h"
#include "ADSR.h"
#include "ADSRDisplay.h"
#include "RollingBuffer.h"

const int kBowedDelayMax = 4096;
const int kBodyModes = 6;
const int kMaxStrings = 4;

struct BowedStringState
{
   // Dual delay: nut-to-bow and bow-to-bridge
   float delayNut[kBowedDelayMax]{};
   float delayBridge[kBowedDelayMax]{};
   int nutLen{ 200 };
   int bridgeLen{ 50 };
   int writeNut{ 0 };
   int writeBridge{ 0 };
   float apNut{ 0 };
   float apBridge{ 0 };

   // String state at bow point
   float vString{ 0 };
   float lastFriction{ 0 };

   // Pitch
   float frequency{ 440 };
   bool active{ false };
};

struct BodyMode
{
   float freq;    // Hz
   float q;       // quality factor
   float gain;    // relative amplitude
   // Biquad state
   float z1{ 0 }, z2{ 0 };
};

class BowedString : public IAudioProcessor, public INoteReceiver, public IDrawableModule,
                    public IFloatSliderListener, public IIntSliderListener
{
public:
   BowedString();
   ~BowedString();
   static IDrawableModule* Create() { return new BowedString(); }
   static bool AcceptsAudio() { return false; }
   static bool AcceptsNotes() { return true; }
   static bool AcceptsPulses() { return false; }

   void CreateUIControls() override;

   void Process(double time) override;
   void SetEnabled(bool enabled) override { mEnabled = enabled; }

   void PlayNote(NoteMessage note) override;
   void SendCC(int control, int value, int voiceIdx = -1) override {}

   void FloatSliderUpdated(FloatSlider* slider, float oldVal, double time) override {}
   void IntSliderUpdated(IntSlider* slider, int oldVal, double time) override {}

   bool IsEnabled() const override { return mEnabled; }
   bool CheckNeedsDraw() override { return true; }

   void LoadLayout(const ofxJSONElement& moduleInfo) override;
   void SetUpFromSaveData() override;

private:
   void DrawModule() override;
   void GetModuleDimensions(float& width, float& height) override;
   void OnClicked(float x, float y, bool right) override;
   void UpdateStringLengths(int stringIdx);
   float AllpassRead(float* buf, int wp, int len, float delay, float& ap);
   float ComputeFriction(float vRel);
   float ProcessBodyMode(BodyMode& mode, float input);

   // Strings
   BowedStringState mStrings[kMaxStrings];
   int mNumStrings{ 1 };

   // Sympathetic string tunings (semitone offsets from main pitch)
   float mSympTuning[kMaxStrings]{ 0, 7, 12, 19 }; // unison, fifth, octave, 12th

   // Bow parameters
   float mBowVelocity{ 0.15f };
   float mBowPressure{ 0.3f };
   float mBowPosition{ 0.12f }; // 0=bridge, 1=nut. 0.12 = normal playing position
   float mBowNoise{ 0.02f };

   // String parameters
   float mStringDamping{ 0.9995f };
   float mBrightness{ 0.7f };

   // Body resonator
   BodyMode mBodyModes[kBodyModes];
   float mBodySize{ 1.0f };
   float mBodyResonance{ 0.85f };

   // Bridge filter state (biquad highpass)
   float mBridgeZ1{ 0 }, mBridgeZ2{ 0 };

   // Sympathetic coupling
   float mSympCoupling{ 0.05f };

   // Amp
   float mVolume{ 0.3f };
   bool mBowing{ false };

   // Viz
   RollingBuffer mStringViz;
   float mHelmholtzCornerPos{ 0 };
   float mBowContactForce{ 0 };

   // Controls
   FloatSlider* mBowVelSlider{ nullptr };
   FloatSlider* mBowPressSlider{ nullptr };
   FloatSlider* mBowPosSlider{ nullptr };
   FloatSlider* mBowNoiseSlider{ nullptr };
   FloatSlider* mDampingSlider{ nullptr };
   FloatSlider* mBrightnessSlider{ nullptr };
   FloatSlider* mBodySizeSlider{ nullptr };
   FloatSlider* mBodyResSlider{ nullptr };
   FloatSlider* mSympSlider{ nullptr };
   IntSlider* mNumStringsSlider{ nullptr };
   FloatSlider* mVolumeSlider{ nullptr };

   ChannelBuffer mWriteBuffer;
};
