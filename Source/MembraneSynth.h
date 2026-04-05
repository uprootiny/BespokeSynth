/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/
//
//  MembraneSynth.h
//
//  2D waveguide mesh on a rectangular grid. Models vibrating membranes:
//  drum heads, plates, gongs. The 2D wave equation creates inharmonic
//  spectra (Bessel/sinusoidal mode shapes) — THIS is what makes drums
//  sound like drums.
//
//  Physics: p[x][y] = c * (p[x-1][y] + p[x+1][y] + p[x][y-1] + p[x][y+1]) / 2
//           - p_prev[x][y]
//  where c is the Courant number (must be <= 1/sqrt(2) for stability).
//
//  Boundary: clamped (p=0 at edges) or free (dp/dn=0 at edges).
//  Strike: delta impulse at (sx, sy), exciting modes based on position.
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

const int kMembraneMaxSize = 16; // 16x16 grid = 256 nodes
const int kMembraneMinSize = 4;

enum MembraneBoundary
{
   kMembrane_Clamped,  // p=0 at edges (drum head bolted to rim)
   kMembrane_Free      // dp/dn=0 at edges (plate resting freely)
};

enum MembraneShape
{
   kShape_Square,
   kShape_Circle       // circular mask on the square grid
};

class MembraneSynth : public IAudioProcessor, public INoteReceiver, public IDrawableModule,
                      public IDropdownListener, public IFloatSliderListener, public IIntSliderListener
{
public:
   MembraneSynth();
   ~MembraneSynth();
   static IDrawableModule* Create() { return new MembraneSynth(); }
   static bool AcceptsAudio() { return false; }
   static bool AcceptsNotes() { return true; }
   static bool AcceptsPulses() { return false; }

   void CreateUIControls() override;

   void Process(double time) override;
   void SetEnabled(bool enabled) override { mEnabled = enabled; }

   void PlayNote(NoteMessage note) override;
   void SendCC(int control, int value, int voiceIdx = -1) override {}

   void DropdownUpdated(DropdownList* list, int oldVal, double time) override {}
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
   void ClearMesh();
   bool IsInside(int x, int y); // for circular mask

   // Mesh state: two buffers (current and previous) for leapfrog integration
   float mP[kMembraneMaxSize][kMembraneMaxSize]{};      // current pressure
   float mPprev[kMembraneMaxSize][kMembraneMaxSize]{};   // previous pressure

   // Visualization buffer (copied once per audio buffer)
   float mPviz[kMembraneMaxSize][kMembraneMaxSize]{};

   int mGridSize{ 10 };
   MembraneBoundary mBoundary{ kMembrane_Clamped };
   MembraneShape mShape{ kShape_Circle };
   float mDamping{ 0.9995f };
   float mTension{ 0.4f };   // Courant number squared: c² <= 0.5 for stability
   float mVolume{ 0.3f };

   // Strike position (normalized 0-1)
   float mStrikeX{ 0.33f };
   float mStrikeY{ 0.4f };

   // Pickup position (where we listen)
   float mPickupX{ 0.5f };
   float mPickupY{ 0.5f };

   // Pitch mapping: tension scales with MIDI pitch
   float mFrequency{ 261.63f };

   ::ADSR mEnvelope;
   ADSRDisplay* mEnvDisplay{ nullptr };
   float mEnvValue{ 0 };

   // Controls
   IntSlider* mGridSizeSlider{ nullptr };
   DropdownList* mBoundaryDropdown{ nullptr };
   DropdownList* mShapeDropdown{ nullptr };
   FloatSlider* mDampingSlider{ nullptr };
   FloatSlider* mTensionSlider{ nullptr };
   FloatSlider* mStrikeXSlider{ nullptr };
   FloatSlider* mStrikeYSlider{ nullptr };
   FloatSlider* mPickupXSlider{ nullptr };
   FloatSlider* mPickupYSlider{ nullptr };
   FloatSlider* mVolumeSlider{ nullptr };

   ChannelBuffer mWriteBuffer;
};
