/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/
//
//  CohomologySynth.h
//  modularSynth
//
//  A synthesis engine grounded in simplicial cohomology.
//
//  The signal lives on a simplicial complex K. Each p-simplex carries
//  a p-cochain value (a float). The coboundary operator δ: C^p → C^{p+1}
//  propagates signal up dimensions. The Laplacian Δ = δδ* + δ*δ
//  governs diffusion, and its spectrum determines the resonances.
//
//  Musical meaning: the Betti numbers β_p = dim H^p(K) count independent
//  resonant modes at each dimension. β₀ = connected components (fundamental),
//  β₁ = independent loops (harmonics), β₂ = enclosed cavities (formants).
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
// MATHEMATICAL FRAMEWORK
// ============================================================
//
// A simplicial complex K consists of:
//   0-simplices (vertices):  σ₀ᵢ            — oscillator nodes
//   1-simplices (edges):     σ₁ᵢⱼ = [i,j]   — waveguide connections
//   2-simplices (faces):     σ₂ᵢⱼₖ = [i,j,k] — resonant cavities
//
// A p-cochain is a function f: {p-simplices} → R.
//   C⁰ = functions on vertices = node amplitudes
//   C¹ = functions on edges = wave flows
//   C² = functions on faces = cavity pressures
//
// The coboundary operator δ: Cᵖ → Cᵖ⁺¹:
//   (δf)(σ) = Σᵢ (-1)ⁱ f(dᵢσ)
//   where dᵢ deletes the i-th vertex of σ.
//
// Concretely:
//   (δ⁰f)[i,j] = f(j) - f(i)               — gradient on edges
//   (δ¹g)[i,j,k] = g[j,k] - g[i,k] + g[i,j] — curl around faces
//
// The adjoint δ*: Cᵖ⁺¹ → Cᵖ (divergence):
//   (δ⁰*g)(i) = Σⱼ g[i,j]                   — sum of flows into vertex
//   (δ¹*h)[i,j] = Σₖ h[i,j,k]               — sum of face pressures on edge
//
// The Hodge Laplacian Δₚ = δₚ₋₁ δₚ₋₁* + δₚ* δₚ:
//   Δ₀ = δ⁰*δ⁰           — graph Laplacian (vertices)
//   Δ₁ = δ⁰δ⁰* + δ¹*δ¹   — edge Laplacian
//   Δ₂ = δ¹δ¹*            — face Laplacian
//
// Harmonic forms: ker(Δₚ) ≅ Hᵖ(K;R) by Hodge theorem.
//   dim ker(Δ₀) = β₀ = # connected components
//   dim ker(Δ₁) = β₁ = # independent loops
//   dim ker(Δ₂) = β₂ = # enclosed cavities
//
// DSP INTERPRETATION:
//   The Laplacian eigenvalues λᵢ are the resonant frequencies.
//   The eigenvectors are the mode shapes.
//   Excitation projects onto modes; each mode rings at √λᵢ.
//   Damping reduces amplitude per mode: aᵢ(t) = aᵢ(0) · e^{-γt}
//
// ARTIFACTS & CONSTRAINTS:
//   - Complex with N₀ vertices, N₁ edges, N₂ faces:
//     Memory: O(N₀ + N₁ + N₂) cochains + O(N₁²) for Laplacian
//   - Eigendecomposition: O(N³) — done once on topology change
//   - Per-sample: O(N_modes) multiply-accumulate — very efficient
//   - Pitch accuracy: eigenvalues are exact (no delay quantization!)
//   - Aliasing: only from nonlinear corruption, not from the linear system
//
// ============================================================

const int kMaxVertices = 12;
const int kMaxEdges = 30;       // complete graph on 8 vertices = 28 edges
const int kMaxFaces = 20;
const int kMaxModes = kMaxVertices + kMaxEdges; // upper bound on total modes

struct Simplex0
{
   int v;                       // vertex index
};

struct Simplex1
{
   int v0, v1;                  // ordered pair, v0 < v1
   int edgeIndex;               // index into edge array
};

struct Simplex2
{
   int v0, v1, v2;              // ordered triple, v0 < v1 < v2
   int faceIndex;               // index into face array
};

// A resonant mode extracted from the Laplacian spectrum
struct CohomMode
{
   float frequency;             // Hz, from √λ
   float amplitude;             // current amplitude
   float phase;                 // current phase (for viz)
   float sinState;              // quadrature oscillator: sin component
   float cosState;              // quadrature oscillator: cos component
   float damping;               // per-sample decay
   int dimension;               // which Cᵖ this mode lives in
   float modeShape[kMaxVertices + kMaxEdges + kMaxFaces]; // eigenvector
   int modeShapeSize;
};

enum ComplexPreset
{
   kPreset_Triangle,            // simplest: 3 vertices, 3 edges, 1 face
   kPreset_Tetrahedron,         // 4v, 6e, 4f — β₀=1, β₁=0, β₂=1
   kPreset_Octahedron,          // 6v, 12e, 8f — β₂=1 (sphere-like)
   kPreset_Torus,               // triangulated torus — β₁=2, β₂=1
   kPreset_KleinBottle,         // β₁=1, β₂=0 (non-orientable)
   kPreset_Bouquet,             // wedge of circles — high β₁
   kPreset_Suspension,          // suspension of a graph — creates β₂
   kPreset_Custom
};

class CohomologySynth : public IAudioProcessor, public INoteReceiver, public IDrawableModule,
                        public IDropdownListener, public IFloatSliderListener, public IIntSliderListener
{
public:
   CohomologySynth();
   ~CohomologySynth();
   static IDrawableModule* Create() { return new CohomologySynth(); }
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

   void DropdownUpdated(DropdownList* list, int oldVal, double time) override;
   void FloatSliderUpdated(FloatSlider* slider, float oldVal, double time) override {}
   void IntSliderUpdated(IntSlider* slider, int oldVal, double time) override {}

   bool IsEnabled() const override { return mEnabled; }
   bool CheckNeedsDraw() override { return true; }

   void LoadLayout(const ofxJSONElement& moduleInfo) override;
   void SetUpFromSaveData() override;

private:
   // IDrawableModule
   void DrawModule() override;
   void GetModuleDimensions(float& width, float& height) override;
   void OnClicked(float x, float y, bool right) override;

   // Topology construction
   void BuildComplex(ComplexPreset preset);
   void AddVertex(int v);
   void AddEdge(int v0, int v1);
   void AddFace(int v0, int v1, int v2);
   int FindEdge(int v0, int v1);

   // Cohomology computation
   void ComputeLaplacianSpectrum();
   void ComputeCoboundary0();   // δ⁰: C⁰ → C¹
   void ComputeCoboundary1();   // δ¹: C¹ → C²
   void ExtractModes();

   // DSP
   void ExciteModes(float velocity, int exciteVertex);

   // Simplicial complex
   int mNumVertices{ 0 };
   int mNumEdges{ 0 };
   int mNumFaces{ 0 };
   Simplex1 mEdges[kMaxEdges];
   Simplex2 mFaces[kMaxFaces];

   // Coboundary matrices (sparse, stored dense for small complexes)
   // δ⁰: N₁ × N₀ matrix
   float mDelta0[kMaxEdges][kMaxVertices]{};
   // δ¹: N₂ × N₁ matrix
   float mDelta1[kMaxFaces][kMaxEdges]{};

   // Laplacian eigenvalues and eigenvectors (vertex Laplacian Δ₀)
   float mEigenvalues[kMaxModes]{};
   float mEigenvectors[kMaxModes][kMaxModes]{};
   int mNumModes{ 0 };

   // Active resonant modes
   CohomMode mModes[kMaxModes];
   int mActiveModes{ 0 };

   // Betti numbers (topological invariants)
   int mBetti[3]{ 0, 0, 0 };   // β₀, β₁, β₂

   // Musical state
   float mPitch{ 60 };
   float mFrequency{ 261.63f };
   float mBaseFreq{ 261.63f };  // fundamental maps to smallest nonzero eigenvalue
   ::ADSR mEnvelope;
   float mEnvelopeValue{ 0 };

   // Parameters
   ComplexPreset mPreset{ kPreset_Tetrahedron };
   float mDamping{ 0.9995f };
   float mBrightness{ 1.0f };   // high-mode amplitude scaling
   float mVolume{ 0.5f };
   float mExciteSpread{ 0.5f }; // how many vertices receive excitation
   int mExciteVertex{ 0 };

   // Vertex positions for visualization (2D projection)
   float mVertexX[kMaxVertices]{};
   float mVertexY[kMaxVertices]{};

   // Per-vertex amplitude for visualization
   float mVertexAmplitude[kMaxVertices]{};

   // UI controls
   DropdownList* mPresetDropdown{ nullptr };
   FloatSlider* mDampingSlider{ nullptr };
   FloatSlider* mBrightnessSlider{ nullptr };
   FloatSlider* mVolumeSlider{ nullptr };
   FloatSlider* mExciteSpreadSlider{ nullptr };
   IntSlider* mExciteVertexSlider{ nullptr };

   ChannelBuffer mWriteBuffer;
};
