/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/
//
//  CoupledOscillators.h
//
//  N spring-coupled harmonic oscillators. Energy transfers between
//  oscillators create beating, phasing, and evolving timbres.
//
//  Physics: m*x''_i + d*x'_i + k_i*x_i = sum_j c*(x_j - x_i)
//  Solved via Störmer-Verlet integration (symplectic, energy-conserving).
//

#pragma once

#include "IAudioProcessor.h"
#include "INoteReceiver.h"
#include "IDrawableModule.h"
#include "Slider.h"
#include "ChannelBuffer.h"
#include "ADSR.h"
#include "ADSRDisplay.h"

const int kMaxOscCount = 8;

struct CoupledMass
{
   float pos{ 0 };          // displacement
   float vel{ 0 };          // velocity
   float freqRatio{ 1.0f }; // natural frequency as ratio to fundamental
   float prevPos{ 0 };      // for viz: smooth transition
};

class CoupledOscillators : public IAudioProcessor, public INoteReceiver, public IDrawableModule,
                           public IFloatSliderListener, public IIntSliderListener
{
public:
   CoupledOscillators();
   ~CoupledOscillators();
   static IDrawableModule* Create() { return new CoupledOscillators(); }
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

   // Physics
   CoupledMass mMasses[kMaxOscCount];
   int mNumMasses{ 4 };
   float mCoupling{ 0.02f };     // spring constant between masses
   float mDamping{ 0.9997f };    // per-sample velocity decay
   float mSpread{ 0.05f };       // frequency detuning between masses
   float mFrequency{ 261.63f };
   int mExciteMass{ 0 };

   // Amp
   ::ADSR mEnvelope;
   ADSRDisplay* mEnvDisplay{ nullptr };
   float mVolume{ 0.5f };
   float mEnvValue{ 0 };

   // Controls
   IntSlider* mNumMassesSlider{ nullptr };
   FloatSlider* mCouplingSlider{ nullptr };
   FloatSlider* mDampingSlider{ nullptr };
   FloatSlider* mSpreadSlider{ nullptr };
   FloatSlider* mVolumeSlider{ nullptr };
   IntSlider* mExciteMassSlider{ nullptr };

   ChannelBuffer mWriteBuffer;
};
