/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/
//
//  TopologyDelay.h
//
//  Delay effect with lattice-diffused feedback. Echoes don't just repeat —
//  they scatter through a small waveguide lattice in the feedback path,
//  creating spatial depth that evolves over time.
//

#pragma once

#include "IAudioProcessor.h"
#include "IDrawableModule.h"
#include "Slider.h"
#include "DropdownList.h"

const int kDelayMaxSamples = 96000; // 2 seconds at 48kHz
const int kDelayLatticeNodes = 4;
const int kDelayNodeBuf = 512;

struct DelayLatticeNode
{
   float fwd{ 0 }, bwd{ 0 };
   float buf[kDelayNodeBuf]{};
   float bufBack[kDelayNodeBuf]{};
   int writePos{ 0 };
   int len{ 100 };
   float apFwd{ 0 }, apBwd{ 0 };
};

class TopologyDelay : public IAudioProcessor, public IDrawableModule,
                      public IFloatSliderListener, public IDropdownListener
{
public:
   TopologyDelay();
   ~TopologyDelay();
   static IDrawableModule* Create() { return new TopologyDelay(); }
   static bool AcceptsAudio() { return true; }
   static bool AcceptsNotes() { return false; }
   static bool AcceptsPulses() { return false; }

   void CreateUIControls() override;
   void Process(double time) override;
   void SetEnabled(bool enabled) override { mEnabled = enabled; }

   void FloatSliderUpdated(FloatSlider* slider, float oldVal, double time) override {}
   void DropdownUpdated(DropdownList* list, int oldVal, double time) override {}

   bool IsEnabled() const override { return mEnabled; }
   void LoadLayout(const ofxJSONElement& moduleInfo) override;
   void SetUpFromSaveData() override;

private:
   void DrawModule() override;
   void GetModuleDimensions(float& width, float& height) override;

   // Main delay line
   float mDelayBuf[kDelayMaxSamples]{};
   int mDelayWritePos{ 0 };
   float mDelayTime{ 0.25f };  // seconds
   float mFeedback{ 0.5f };
   float mWetDry{ 0.4f };

   // Lattice in the feedback path
   DelayLatticeNode mNodes[kDelayLatticeNodes];
   float mDiffusion{ 0.3f };  // how much the lattice smears the feedback
   float mDamping{ 0.995f };

   // Viz
   float mNodeViz[kDelayLatticeNodes]{};

   // Controls
   FloatSlider* mDelayTimeSlider{ nullptr };
   FloatSlider* mFeedbackSlider{ nullptr };
   FloatSlider* mDiffusionSlider{ nullptr };
   FloatSlider* mDampingSlider{ nullptr };
   FloatSlider* mWetDrySlider{ nullptr };
};
