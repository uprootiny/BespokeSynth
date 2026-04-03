/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/
//
//  TopologySynth.h
//
//  A skeuomorphic multistage synthesizer combining waveguide lattice
//  resonance with modal filtering, corruption, and an ADSR envelope.
//
//  Architecture:
//    [Note] → Exciter → Lattice (π₁) → Shaper → Modal Filter (H*) → Amp → [Out]
//
//  Each stage is drawn as a distinct panel with custom NanoVG rendering.
//

#pragma once

#include "IAudioProcessor.h"
#include "INoteReceiver.h"
#include "IDrawableModule.h"
#include "Slider.h"
#include "DropdownList.h"
#include "ChannelBuffer.h"
#include "ADSR.h"
#include "ADSRDisplay.h"
#include "RollingBuffer.h"

const int kTopoMaxNodes = 8;
const int kTopoDelayMax = 2048;
const int kTopoMaxModes = 8;

// --- Stage 1: Exciter ---
enum TopoExciterType
{
   kExciter_Noise,
   kExciter_Impulse,
   kExciter_Saw
};

// --- Stage 2: Lattice ---
enum TopoBoundary
{
   kTopo_Fixed,
   kTopo_Ring,
   kTopo_Mobius
};

// --- Stage 3: Shaper ---
enum TopoShaperType
{
   kShaper_None,
   kShaper_Tanh,
   kShaper_Fold,
   kShaper_Rectify
};

struct TopoNode
{
   float fwd{ 0 };
   float bwd{ 0 };
   float delayFwd[kTopoDelayMax]{};
   float delayBwd[kTopoDelayMax]{};
   int delayLen{ 100 };
   int writePos{ 0 };
   float apStateFwd{ 0 };
   float apStateBwd{ 0 };
   float dcState{ 0 };
};

class TopologySynth : public IAudioProcessor, public INoteReceiver, public IDrawableModule,
                      public IDropdownListener, public IFloatSliderListener, public IIntSliderListener
{
public:
   TopologySynth();
   ~TopologySynth();
   static IDrawableModule* Create() { return new TopologySynth(); }
   static bool AcceptsAudio() { return false; }
   static bool AcceptsNotes() { return true; }
   static bool AcceptsPulses() { return false; }

   void CreateUIControls() override;

   void Process(double time) override;
   void SetEnabled(bool enabled) override { mEnabled = enabled; }

   void PlayNote(NoteMessage note) override;
   void SendCC(int control, int value, int voiceIdx = -1) override {}

   void DropdownUpdated(DropdownList* list, int oldVal, double time) override;
   void FloatSliderUpdated(FloatSlider* slider, float oldVal, double time) override {}
   void IntSliderUpdated(IntSlider* slider, int oldVal, double time) override;

   bool IsEnabled() const override { return mEnabled; }

   void LoadLayout(const ofxJSONElement& moduleInfo) override;
   void SetUpFromSaveData() override;

private:
   void DrawModule() override;
   void GetModuleDimensions(float& width, float& height) override;

   // DSP internals
   void UpdateDelayLengths();
   float AllpassRead(float* buf, int wp, int len, float delay, float& apState);
   float ApplyShaper(float x, TopoShaperType type, float drive);
   void ClearState();

   // --- Stage 1: Exciter ---
   TopoExciterType mExciterType{ kExciter_Noise };
   float mExciteDecay{ 0.97f };
   float mExciteAmount{ 0 };

   // --- Stage 2: Lattice ---
   TopoNode mNodes[kTopoMaxNodes];
   int mNumNodes{ 6 };
   TopoBoundary mBoundary{ kTopo_Ring };
   float mDamping{ 0.9985f };
   float mReflection{ 0.15f };
   int mExciteNode{ 1 };

   // --- Stage 3: Shaper ---
   TopoShaperType mShaperType{ kShaper_Tanh };
   float mShaperDrive{ 1.5f };
   float mShaperMix{ 0.3f };

   // --- Stage 4: Modal filter (simplified: 3 resonant bandpass modes) ---
   float mModalFreqs[3]{ 1.0f, 2.7f, 4.2f }; // ratios to fundamental
   float mModalAmps[3]{ 1.0f, 0.5f, 0.25f };
   float mModalQ{ 10.0f };
   // Biquad state per mode
   float mModalZ1[3]{};
   float mModalZ2[3]{};
   float mModalBrightness{ 0.7f };

   // --- Stage 5: Amp ---
   ::ADSR mAmpEnv;
   ADSRDisplay* mAmpEnvDisplay{ nullptr };
   float mVolume{ 0.5f };
   float mEnvValue{ 0 };

   // Musical state
   float mPitch{ 60 };
   float mFrequency{ 261.63f };
   bool mNoteOn{ false };

   // Visualization: small output scope
   RollingBuffer mScopeBuffer;

   // Per-node viz
   float mNodeViz[kTopoMaxNodes]{};

   // Controls
   DropdownList* mExciterDropdown{ nullptr };
   FloatSlider* mExciteDecaySlider{ nullptr };
   IntSlider* mNumNodesSlider{ nullptr };
   IntSlider* mExciteNodeSlider{ nullptr };
   DropdownList* mBoundaryDropdown{ nullptr };
   FloatSlider* mDampingSlider{ nullptr };
   FloatSlider* mReflectionSlider{ nullptr };
   DropdownList* mShaperDropdown{ nullptr };
   FloatSlider* mShaperDriveSlider{ nullptr };
   FloatSlider* mShaperMixSlider{ nullptr };
   FloatSlider* mModalBrightnessSlider{ nullptr };
   FloatSlider* mModalQSlider{ nullptr };
   FloatSlider* mVolumeSlider{ nullptr };

   ChannelBuffer mWriteBuffer;
};
