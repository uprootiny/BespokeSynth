/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/
//
//  TopologyFilter.h
//
//  Audio effect: routes input audio through a waveguide lattice.
//  The lattice topology shapes which frequencies resonate.
//  Note input tunes the lattice to follow pitch.
//

#pragma once

#include "IAudioProcessor.h"
#include "INoteReceiver.h"
#include "IDrawableModule.h"
#include "Slider.h"
#include "DropdownList.h"
#include "ChannelBuffer.h"

const int kFiltMaxNodes = 8;
const int kFiltDelayMax = 2048;

enum FiltBoundary { kFilt_Fixed, kFilt_Ring, kFilt_Mobius };
enum FiltCorruption { kFiltCorr_None, kFiltCorr_Tanh, kFiltCorr_Fold };

struct FiltNode
{
   float fwd{ 0 };
   float bwd{ 0 };
   float delayFwd[kFiltDelayMax]{};
   float delayBwd[kFiltDelayMax]{};
   int delayLen{ 100 };
   int writePos{ 0 };
   float apFwd{ 0 };
   float apBwd{ 0 };
};

class TopologyFilter : public IAudioProcessor, public INoteReceiver, public IDrawableModule,
                       public IDropdownListener, public IFloatSliderListener, public IIntSliderListener
{
public:
   TopologyFilter();
   ~TopologyFilter();
   static IDrawableModule* Create() { return new TopologyFilter(); }
   static bool AcceptsAudio() { return true; }
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
   void UpdateDelayLengths();
   float AllpassRead(float* buf, int wp, int len, float delay, float& apState);

   // Lattice
   FiltNode mNodes[kFiltMaxNodes];
   int mNumNodes{ 5 };
   FiltBoundary mBoundary{ kFilt_Ring };
   float mDamping{ 0.997f };
   float mReflection{ 0.2f };
   float mDrive{ 1.0f };
   int mCorruptionType{ 0 };
   int mInjectNode{ 0 };
   int mPickupNode{ 0 };
   float mWetDry{ 0.5f };

   // Pitch tracking
   float mFrequency{ 261.63f };

   // Viz
   float mNodeViz[kFiltMaxNodes]{};

   // Controls
   IntSlider* mNumNodesSlider{ nullptr };
   DropdownList* mBoundaryDropdown{ nullptr };
   FloatSlider* mDampingSlider{ nullptr };
   FloatSlider* mReflectionSlider{ nullptr };
   FloatSlider* mDriveSlider{ nullptr };
   DropdownList* mCorruptionDropdown{ nullptr };
   IntSlider* mInjectSlider{ nullptr };
   IntSlider* mPickupSlider{ nullptr };
   FloatSlider* mWetDrySlider{ nullptr };
};
