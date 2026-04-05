/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/
//
//  WavetableMorph.h
//
//  Wavetable synthesizer with 4 morphable waveform slots.
//  Crossfade between slots via a single morph parameter.
//  Each slot: 256-sample single-cycle waveform, drawn or preset.
//  Smooth interpolation between slots creates timbral movement.
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

const int kWaveSize = 256;
const int kWaveSlots = 4;

class WavetableMorph : public IAudioProcessor, public INoteReceiver, public IDrawableModule,
                       public IFloatSliderListener, public IIntSliderListener, public IDropdownListener
{
public:
   WavetableMorph();
   ~WavetableMorph();
   static IDrawableModule* Create() { return new WavetableMorph(); }
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
   void DropdownUpdated(DropdownList* list, int oldVal, double time) override;

   bool IsEnabled() const override { return mEnabled; }
   bool CheckNeedsDraw() override { return true; }

   void LoadLayout(const ofxJSONElement& moduleInfo) override;
   void SetUpFromSaveData() override;

private:
   void DrawModule() override;
   void GetModuleDimensions(float& width, float& height) override;
   void OnClicked(float x, float y, bool right) override;
   void InitWaveforms();
   float SampleWavetable(float phase, float morph);

   // 4 waveform slots
   float mWaves[kWaveSlots][kWaveSize]{};

   // Oscillator state
   float mPhase{ 0 };
   float mFrequency{ 261.63f };

   // Parameters
   float mMorph{ 0 };          // 0-3: crossfade position across the 4 slots
   float mDetune{ 0 };         // cents, ±100
   float mVolume{ 0.3f };
   int mUnisonCount{ 1 };      // 1-4 unison voices
   float mUnisonSpread{ 0.1f };// detune spread for unison

   // Per-unison voice state
   float mUniPhase[4]{ 0 };
   float mUniDetune[4]{ 0 };

   // Envelope
   ::ADSR mEnvelope;
   ADSRDisplay* mEnvDisplay{ nullptr };
   float mEnvValue{ 0 };

   // Preset waveforms
   int mPreset{ 0 };

   // Controls
   FloatSlider* mMorphSlider{ nullptr };
   FloatSlider* mDetuneSlider{ nullptr };
   FloatSlider* mVolumeSlider{ nullptr };
   IntSlider* mUnisonSlider{ nullptr };
   FloatSlider* mUnisonSpreadSlider{ nullptr };
   DropdownList* mPresetDropdown{ nullptr };

   ChannelBuffer mWriteBuffer;
};
