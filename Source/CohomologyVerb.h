/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/
//
//  CohomologyVerb.h
//
//  Reverb where the reflection structure comes from a simplicial complex.
//  The mixing matrix is derived from the graph Laplacian.
//  Betti numbers shape the character: β₁ = diffusion, β₂ = coloration.
//
//  Architecture: Feedback Delay Network (FDN) with N delay lines.
//  N = number of vertices in the complex. Mixing matrix = normalized Laplacian.
//  Delay lengths from edge weights scaled by room size.
//

#pragma once

#include "IAudioProcessor.h"
#include "IDrawableModule.h"
#include "Slider.h"
#include "DropdownList.h"

const int kVerbMaxDelays = 8;
const int kVerbMaxDelaySamples = 48000; // 1 second at 48kHz

enum VerbPreset
{
   kVerb_Room,       // triangle: 3 delays, minimal diffusion
   kVerb_Hall,       // tetrahedron: 4 delays, enclosed cavity
   kVerb_Chamber,    // octahedron: 6 delays, rich reflections
   kVerb_Cathedral,  // high-β₁ bouquet: many loops, lush diffusion
   kVerb_Cave        // torus: loops + cavity, dark and deep
};

class CohomologyVerb : public IAudioProcessor, public IDrawableModule,
                       public IFloatSliderListener, public IDropdownListener
{
public:
   CohomologyVerb();
   ~CohomologyVerb();
   static IDrawableModule* Create() { return new CohomologyVerb(); }
   static bool AcceptsAudio() { return true; }
   static bool AcceptsNotes() { return false; }
   static bool AcceptsPulses() { return false; }

   void CreateUIControls() override;
   void Process(double time) override;
   void SetEnabled(bool enabled) override { mEnabled = enabled; }

   void FloatSliderUpdated(FloatSlider* slider, float oldVal, double time) override {}
   void DropdownUpdated(DropdownList* list, int oldVal, double time) override;

   bool IsEnabled() const override { return mEnabled; }
   bool CheckNeedsDraw() override { return true; }
   void LoadLayout(const ofxJSONElement& moduleInfo) override;
   void SetUpFromSaveData() override;

private:
   void DrawModule() override;
   void GetModuleDimensions(float& width, float& height) override;
   void BuildFDN(VerbPreset preset);

   // FDN state
   float mDelayBufs[kVerbMaxDelays][kVerbMaxDelaySamples]{};
   int mDelayLens[kVerbMaxDelays]{};
   int mWritePos[kVerbMaxDelays]{};
   float mMixMatrix[kVerbMaxDelays][kVerbMaxDelays]{}; // from Laplacian
   int mNumDelays{ 4 };

   // Parameters
   VerbPreset mPreset{ kVerb_Hall };
   float mSize{ 0.5f };       // scales delay lengths
   float mDecay{ 0.85f };     // per-reflection decay
   float mDamping{ 0.7f };    // HF absorption (lowpass in feedback)
   float mWetDry{ 0.35f };
   float mPreDelay{ 0.01f };  // seconds

   // HF damping state (one-pole lowpass per delay line)
   float mDampState[kVerbMaxDelays]{};

   // Pre-delay
   float mPreDelayBuf[kVerbMaxDelaySamples]{};
   int mPreDelayWritePos{ 0 };

   // Betti numbers of current preset (for display)
   int mBetti[3]{};

   // Viz: energy per delay line
   float mDelayViz[kVerbMaxDelays]{};

   // Vertex positions for viz
   float mVertX[kVerbMaxDelays]{};
   float mVertY[kVerbMaxDelays]{};

   // Controls
   DropdownList* mPresetDropdown{ nullptr };
   FloatSlider* mSizeSlider{ nullptr };
   FloatSlider* mDecaySlider{ nullptr };
   FloatSlider* mDampingSlider{ nullptr };
   FloatSlider* mPreDelaySlider{ nullptr };
   FloatSlider* mWetDrySlider{ nullptr };
};
