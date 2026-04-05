/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/
//
//  FMCluster.h
//
//  Graph-based FM synthesizer. N oscillators where ANY can modulate ANY.
//  The modulation graph IS the algorithm. Draw the graph, hear the timbre.
//
//  DSP: out_i = sin(phase_i + sum_j(depth_ij * sin(phase_j)))
//  Cost: N sines + N² MACs per sample. N≤6 → 6 sines + 36 MACs. Cheap.
//

#pragma once

#include "IAudioProcessor.h"
#include "INoteReceiver.h"
#include "IDrawableModule.h"
#include "Slider.h"
#include "ChannelBuffer.h"
#include "ADSR.h"
#include "ADSRDisplay.h"

const int kFMMaxOps = 6;

struct FMOperator
{
   float phase{ 0 };
   float ratio{ 1.0f };        // frequency ratio to fundamental
   float outputLevel{ 0 };     // 0 = modulator only, 1 = carrier (heard)
   float prevOutput{ 0 };      // for feedback
};

class FMCluster : public IAudioProcessor, public INoteReceiver, public IDrawableModule,
                  public IFloatSliderListener, public IIntSliderListener
{
public:
   FMCluster();
   ~FMCluster();
   static IDrawableModule* Create() { return new FMCluster(); }
   static bool AcceptsAudio() { return false; }
   static bool AcceptsNotes() { return true; }
   static bool AcceptsPulses() { return false; }

   void CreateUIControls() override;

   void Process(double time) override;
   void SetEnabled(bool enabled) override { mEnabled = enabled; }

   void PlayNote(NoteMessage note) override;
   void SendCC(int control, int value, int voiceIdx = -1) override {}

   void FloatSliderUpdated(FloatSlider* slider, float oldVal, double time) override {}
   void IntSliderUpdated(IntSlider* slider, int oldVal, double time) override;

   bool IsEnabled() const override { return mEnabled; }
   bool CheckNeedsDraw() override { return true; }

   void LoadLayout(const ofxJSONElement& moduleInfo) override;
   void SetUpFromSaveData() override;

private:
   void DrawModule() override;
   void GetModuleDimensions(float& width, float& height) override;
   void OnClicked(float x, float y, bool right) override;

   // Operators
   FMOperator mOps[kFMMaxOps];
   int mNumOps{ 4 };

   // Modulation matrix: depth[i][j] = how much op j modulates op i
   float mModDepth[kFMMaxOps][kFMMaxOps]{};

   // Global params
   float mFrequency{ 261.63f };
   float mVolume{ 0.3f };
   float mFeedback{ 0.0f };     // self-modulation amount (applied to diagonal)
   float mBrightness{ 1.0f };   // scales all modulation depths

   // Envelope
   ::ADSR mEnvelope;
   ADSRDisplay* mEnvDisplay{ nullptr };
   float mEnvValue{ 0 };

   // Viz: operator output amplitudes
   float mOpViz[kFMMaxOps]{};

   // Controls
   IntSlider* mNumOpsSlider{ nullptr };
   FloatSlider* mFeedbackSlider{ nullptr };
   FloatSlider* mBrightnessSlider{ nullptr };
   FloatSlider* mVolumeSlider{ nullptr };

   // Per-operator ratio sliders
   FloatSlider* mRatioSliders[kFMMaxOps]{ nullptr };
   FloatSlider* mLevelSliders[kFMMaxOps]{ nullptr };

   ChannelBuffer mWriteBuffer;
};
