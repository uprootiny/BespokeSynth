/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/
//
//  LatticeSynth.h
//  modularSynth
//
//  Waveguide lattice synthesizer with topological boundary conditions
//  and local nonlinear corruptions. The lattice's fundamental group
//  determines its resonant structure; corruptions break symmetry and
//  create harmonic complexity.
//

#pragma once

#include "IAudioProcessor.h"
#include "INoteReceiver.h"
#include "IDrawableModule.h"
#include "Slider.h"
#include "DropdownList.h"
#include "ChannelBuffer.h"
#include "ADSR.h"

// ============================================================
// DSP DATA SHAPES
// ============================================================
//
// At each node i, state is a pair (forward, backward) traveling waves:
//   a+_i : float  (rightward-traveling pressure wave)
//   a-_i : float  (leftward-traveling pressure wave)
//
// These form a section of a rank-2 vector bundle over the graph.
//
// SCATTERING JUNCTION at node i:
//   [a+_out]   [1-r   r ] [a+_in]
//   [a-_out] = [ r   1-r] [a-_in]
//   where r in [0,1] is the reflection coefficient.
//   r=0: full transmission (transparent node)
//   r=1: full reflection (hard wall)
//   Energy is preserved: |a+_out|² + |a-_out|² = |a+_in|² + |a-_in|²
//
// DELAY LINES between nodes:
//   Each edge (i, i+1) carries a delay of L_i samples.
//   For pitched sound: sum of all delays in a loop = sr/f0
//   Fractional delay via linear interpolation (allpass for higher quality).
//
// BOUNDARY CONDITIONS (encode π₁ of the lattice space):
//   Fixed:   a+(N) = -a+(N)       π₁ = {e}        (trivial, no loops)
//   Free:    a+(N) = +a+(N)       π₁ = {e}        (trivial)
//   Ring:    a+(N) = a+(0)        π₁ = Z           (integer winding)
//   Möbius:  a+(N) = -a+(0)       π₁ = Z           (winding + phase flip)
//
// CORRUPTIONS (local nonlinearities at node i):
//   None:      f(x) = x
//   SoftClip:  f(x) = tanh(g * x)           — odd harmonics, warm
//   Fold:      f(x) = |2*frac(x/2+.25)-1|   — rich harmonics, metallic
//   Rectify:   f(x) = |x|                   — even harmonics, hollow
//
// ARTIFACTS:
//   - Delay quantization → pitch error (max 1 sample = 1200*log2(sr/(sr-1)) cents)
//   - Nonlinearities → aliasing (mitigated by internal oversampling)
//   - Energy accumulation at corruption sites → DC offset (DC blocker per node)
//   - Möbius boundary → fundamental at half the ring frequency (octave down)
//
// ============================================================

const int kMaxLatticeNodes = 16;
const int kMaxDelayLength = 4096;

enum LatticeBoundary
{
   kBoundary_Fixed,    // hard wall: reflects with sign inversion
   kBoundary_Free,     // open end: reflects without inversion
   kBoundary_Ring,     // periodic: wraps around (torus in 1D = circle)
   kBoundary_Mobius    // periodic + phase inversion (Möbius strip)
};

enum LatticeCorruption
{
   kCorruption_None,
   kCorruption_SoftClip,
   kCorruption_Fold,
   kCorruption_Rectify
};

struct LatticeNode
{
   float forward{ 0 };           // a+ traveling wave
   float backward{ 0 };          // a- traveling wave
   float reflection{ 0.0f };     // scattering coefficient r ∈ [0,1]
   LatticeCorruption corruption{ kCorruption_None };
   float corruptionDrive{ 1.0f };

   // Delay line to next node (fractional delay via linear interp)
   float delayBuffer[kMaxDelayLength]{};
   float delayBufferBack[kMaxDelayLength]{};
   int delayLength{ 100 };
   int writePos{ 0 };

   // DC blocker state (1-pole highpass at ~20Hz)
   float dcState{ 0 };

   // Allpass interpolation state (per-edge, for fractional delay)
   float allpassStateFwd{ 0 };
   float allpassStateBack{ 0 };
};

class LatticeSynth : public IAudioProcessor, public INoteReceiver, public IDrawableModule,
                     public IDropdownListener, public IFloatSliderListener, public IIntSliderListener
{
public:
   LatticeSynth();
   ~LatticeSynth();
   static IDrawableModule* Create() { return new LatticeSynth(); }
   static bool AcceptsAudio() { return false; }
   static bool AcceptsNotes() { return true; }
   static bool AcceptsPulses() { return false; }

   void CreateUIControls() override;

   // IAudioSource
   void Process(double time) override;
   void SetEnabled(bool enabled) override { mEnabled = enabled; }

   // INoteReceiver
   void PlayNote(NoteMessage note) override;
   void SendCC(int control, int value, int voiceIdx = -1) override {}

   void DropdownUpdated(DropdownList* list, int oldVal, double time) override {}
   void FloatSliderUpdated(FloatSlider* slider, float oldVal, double time) override {}
   void IntSliderUpdated(IntSlider* slider, int oldVal, double time) override;

   bool IsEnabled() const override { return mEnabled; }

   void LoadLayout(const ofxJSONElement& moduleInfo) override;
   void SetUpFromSaveData() override;

private:
   // IDrawableModule
   void DrawModule() override;
   void GetModuleDimensions(float& width, float& height) override;

   // DSP
   void ExciteNode(int node, float amount);
   float ApplyCorruption(float sample, LatticeCorruption type, float drive);
   void ScatterAtNode(int i);
   void PropagateForward(int from, int to);
   void PropagateBackward(int from, int to);
   void ApplyBoundaryConditions();
   void UpdateDelayLengths();
   float ReadDelay(float* buffer, int writePos, int length, float samplesAgo, float& allpassState);

   // Lattice state
   LatticeNode mNodes[kMaxLatticeNodes];
   int mNumNodes{ 8 };
   LatticeBoundary mBoundary{ kBoundary_Ring };
   float mDamping{ 0.998f };      // per-sample energy loss
   float mPitch{ 60 };            // current MIDI pitch
   float mFrequency{ 261.63f };   // Hz
   float mExciteAmount{ 0 };      // current excitation amplitude
   ::ADSR mEnvelope;
   float mEnvelopeValue{ 0 };

   // Visualization state (sampled from DSP for drawing)
   float mNodeAmplitudes[kMaxLatticeNodes]{};
   float mNodeEnergies[kMaxLatticeNodes]{};

   // Controls
   int mExciteNode{ 0 };          // which node receives excitation
   float mCorruptionDrive{ 1.5f };
   float mVolume{ 0.5f };
   float mReflection{ 0.0f };     // global reflection coefficient
   int mCorruptionType{ 0 };

   // UI
   IntSlider* mNumNodesSlider{ nullptr };
   IntSlider* mExciteNodeSlider{ nullptr };
   FloatSlider* mDampingSlider{ nullptr };
   FloatSlider* mReflectionSlider{ nullptr };
   FloatSlider* mCorruptionDriveSlider{ nullptr };
   FloatSlider* mVolumeSlider{ nullptr };
   DropdownList* mBoundaryDropdown{ nullptr };
   DropdownList* mCorruptionDropdown{ nullptr };

   ChannelBuffer mWriteBuffer;
};
