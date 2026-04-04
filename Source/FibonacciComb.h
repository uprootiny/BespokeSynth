/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/
//
//  FibonacciComb.h
//
//  Comb filter with delay taps at Fibonacci-numbered sample positions.
//  Peaks converge to the golden ratio φ = (1+√5)/2 ≈ 1.618.
//  The "most irrational" spectrum — maximally spread, non-harmonic.
//  Crystalline, metallic, bell-like character.
//

#pragma once

#include "IAudioProcessor.h"
#include "IDrawableModule.h"
#include "Slider.h"

const int kFibMaxTaps = 14;           // F(14) = 377
const int kFibMaxDelay = 48000;       // 1 second at 48kHz

class FibonacciComb : public IAudioProcessor, public IDrawableModule,
                      public IFloatSliderListener, public IIntSliderListener
{
public:
   FibonacciComb();
   ~FibonacciComb();
   static IDrawableModule* Create() { return new FibonacciComb(); }
   static bool AcceptsAudio() { return true; }
   static bool AcceptsNotes() { return false; }
   static bool AcceptsPulses() { return false; }

   void CreateUIControls() override;
   void Process(double time) override;
   void SetEnabled(bool enabled) override { mEnabled = enabled; }

   void FloatSliderUpdated(FloatSlider* slider, float oldVal, double time) override {}
   void IntSliderUpdated(IntSlider* slider, int oldVal, double time) override {}

   bool IsEnabled() const override { return mEnabled; }
   void LoadLayout(const ofxJSONElement& moduleInfo) override;
   void SetUpFromSaveData() override;

private:
   void DrawModule() override;
   void GetModuleDimensions(float& width, float& height) override;

   // Fibonacci sequence (precomputed)
   int mFib[kFibMaxTaps];

   // Delay buffer
   float mBuf[kFibMaxDelay]{};
   int mWritePos{ 0 };

   // Parameters
   float mFundamental{ 200 };   // Hz — scales all Fibonacci delays
   int mDepth{ 8 };              // how many Fibonacci taps
   float mFeedback{ 0.6f };
   float mDamping{ 0.3f };       // per-tap HF damping
   float mWetDry{ 0.5f };

   // Damping state per tap
   float mDampState[kFibMaxTaps]{};

   // Viz: per-tap amplitude
   float mTapViz[kFibMaxTaps]{};

   // Controls
   FloatSlider* mFundSlider{ nullptr };
   IntSlider* mDepthSlider{ nullptr };
   FloatSlider* mFeedbackSlider{ nullptr };
   FloatSlider* mDampingSlider{ nullptr };
   FloatSlider* mWetDrySlider{ nullptr };
};
