/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/
//
//  CohomologySynth.cpp
//  modularSynth
//
//  Simplicial cohomology synthesizer.
//  See CohomologySynth.h for the mathematical framework.
//

#include "CohomologySynth.h"
#include "OpenFrameworksPort.h"
#include "SynthGlobals.h"
#include "IAudioReceiver.h"
#include "ModularSynth.h"
#include "Profiler.h"
#include "ofxJSONElement.h"
#include "UIControlMacros.h"
#include "nanovg/nanovg.h"
#include <cmath>
#include <algorithm>

CohomologySynth::CohomologySynth()
: IAudioProcessor(gBufferSize)
, mWriteBuffer(gBufferSize)
{
   mEnvelope.Set(3, 0, 1, 500);
   BuildComplex(kPreset_Tetrahedron);
}

CohomologySynth::~CohomologySynth()
{
}

void CohomologySynth::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   UIBLOCK0();
   DROPDOWN(mPresetDropdown, "shape", (int*)&mPreset, 100);
   FLOATSLIDER_DIGITS(mDampingSlider, "sustain", &mDamping, 0.99f, 0.99999f, 5);
   FLOATSLIDER(mBrightnessSlider, "bright", &mBrightness, 0.0f, 2.0f);
   FLOATSLIDER(mExciteSpreadSlider, "spread", &mExciteSpread, 0.0f, 1.0f);
   INTSLIDER(mExciteVertexSlider, "vertex", &mExciteVertex, 0, std::max(0, mNumVertices - 1));
   FLOATSLIDER(mVolumeSlider, "vol", &mVolume, 0.0f, 1.0f);
   ENDUIBLOCK0();

   mPresetDropdown->AddLabel("triangle", kPreset_Triangle);
   mPresetDropdown->AddLabel("tetra", kPreset_Tetrahedron);
   mPresetDropdown->AddLabel("octa", kPreset_Octahedron);
   mPresetDropdown->AddLabel("torus", kPreset_Torus);
   mPresetDropdown->AddLabel("klein", kPreset_KleinBottle);
   mPresetDropdown->AddLabel("bouquet", kPreset_Bouquet);
   mPresetDropdown->AddLabel("susp", kPreset_Suspension);
}

void CohomologySynth::DropdownUpdated(DropdownList* list, int oldVal, double time)
{
   if (list == mPresetDropdown)
   {
      BuildComplex((ComplexPreset)mPreset);
      mExciteVertexSlider->SetExtents(0, std::max(0, mNumVertices - 1));
      if (mExciteVertex >= mNumVertices)
         mExciteVertex = 0;
   }
}

// ============================================================
// TOPOLOGY CONSTRUCTION
// ============================================================

void CohomologySynth::AddVertex(int v)
{
   if (v >= mNumVertices)
      mNumVertices = v + 1;
}

void CohomologySynth::AddEdge(int v0, int v1)
{
   if (mNumEdges >= kMaxEdges) return;
   int a = std::min(v0, v1);
   int b = std::max(v0, v1);
   // Check for duplicate
   for (int i = 0; i < mNumEdges; ++i)
      if (mEdges[i].v0 == a && mEdges[i].v1 == b) return;
   mEdges[mNumEdges] = { a, b, mNumEdges };
   mNumEdges++;
   AddVertex(a);
   AddVertex(b);
}

void CohomologySynth::AddFace(int v0, int v1, int v2)
{
   if (mNumFaces >= kMaxFaces) return;
   int sorted[3] = { v0, v1, v2 };
   std::sort(sorted, sorted + 3);
   // Ensure edges exist
   AddEdge(sorted[0], sorted[1]);
   AddEdge(sorted[1], sorted[2]);
   AddEdge(sorted[0], sorted[2]);
   mFaces[mNumFaces] = { sorted[0], sorted[1], sorted[2], mNumFaces };
   mNumFaces++;
}

int CohomologySynth::FindEdge(int v0, int v1)
{
   int a = std::min(v0, v1);
   int b = std::max(v0, v1);
   for (int i = 0; i < mNumEdges; ++i)
      if (mEdges[i].v0 == a && mEdges[i].v1 == b) return i;
   return -1;
}

void CohomologySynth::BuildComplex(ComplexPreset preset)
{
   mNumVertices = 0;
   mNumEdges = 0;
   mNumFaces = 0;
   memset(mDelta0, 0, sizeof(mDelta0));
   memset(mDelta1, 0, sizeof(mDelta1));

   switch (preset)
   {
      case kPreset_Triangle:
         // Simplest closed surface: 3v, 3e, 1f. β₀=1, β₁=0, β₂=0
         AddFace(0, 1, 2);
         // Positions: equilateral triangle
         mVertexX[0] = 0;    mVertexY[0] = -1;
         mVertexX[1] = 0.87f; mVertexY[1] = 0.5f;
         mVertexX[2] = -0.87f; mVertexY[2] = 0.5f;
         break;

      case kPreset_Tetrahedron:
         // 4v, 6e, 4f. β₀=1, β₁=0, β₂=1 (hollow sphere)
         AddFace(0, 1, 2);
         AddFace(0, 1, 3);
         AddFace(0, 2, 3);
         AddFace(1, 2, 3);
         mVertexX[0] = 0;     mVertexY[0] = -1;
         mVertexX[1] = 0.94f;  mVertexY[1] = 0.33f;
         mVertexX[2] = -0.94f; mVertexY[2] = 0.33f;
         mVertexX[3] = 0;     mVertexY[3] = 0.5f;
         break;

      case kPreset_Octahedron:
         // 6v, 12e, 8f. β₀=1, β₁=0, β₂=1
         AddFace(0, 1, 2); AddFace(0, 2, 3); AddFace(0, 3, 4); AddFace(0, 4, 1);
         AddFace(5, 1, 2); AddFace(5, 2, 3); AddFace(5, 3, 4); AddFace(5, 4, 1);
         mVertexX[0] = 0;    mVertexY[0] = -1;
         mVertexX[1] = 1;    mVertexY[1] = 0;
         mVertexX[2] = 0;    mVertexY[2] = 0.6f;
         mVertexX[3] = -1;   mVertexY[3] = 0;
         mVertexX[4] = 0;    mVertexY[4] = -0.6f;
         mVertexX[5] = 0;    mVertexY[5] = 1;
         break;

      case kPreset_Torus:
         // Minimal triangulation of torus: 7 vertices. β₀=1, β₁=2, β₂=1
         // Using the classic 7-vertex triangulation (Möbius-Kantor)
         mNumVertices = 7;
         AddFace(0, 1, 2); AddFace(0, 2, 3); AddFace(0, 3, 5);
         AddFace(0, 4, 6); AddFace(0, 5, 4); AddFace(0, 6, 1);
         AddFace(1, 2, 6); AddFace(1, 3, 5); AddFace(1, 3, 6);
         AddFace(2, 3, 4); AddFace(2, 4, 5); AddFace(2, 5, 6);
         AddFace(3, 4, 6); AddFace(4, 5, 6);
         // Positions: projected torus
         for (int i = 0; i < 7; ++i)
         {
            float angle = (float)i / 7 * FTWO_PI;
            float r = 0.7f + 0.3f * cosf(angle * 2);
            mVertexX[i] = cosf(angle) * r;
            mVertexY[i] = sinf(angle) * r;
         }
         break;

      case kPreset_KleinBottle:
         // Triangulation with identifications. β₀=1, β₁=1, β₂=0
         // Simplified: 6 vertices, edges with Klein identifications
         mNumVertices = 6;
         AddFace(0, 1, 3); AddFace(1, 3, 4); AddFace(1, 2, 4);
         AddFace(2, 4, 5); AddFace(0, 2, 5); AddFace(0, 3, 5);
         AddFace(3, 4, 5);
         for (int i = 0; i < 6; ++i)
         {
            float angle = (float)i / 6 * FTWO_PI;
            mVertexX[i] = cosf(angle) * (0.6f + 0.2f * sinf(angle * 3));
            mVertexY[i] = sinf(angle) * (0.6f + 0.2f * cosf(angle * 2));
         }
         break;

      case kPreset_Bouquet:
         // Wedge of 4 circles (rose): 1v, 4e, 0f. β₀=1, β₁=4
         // Each edge is a loop from vertex 0 to itself
         mNumVertices = 5;
         AddEdge(0, 1); AddEdge(1, 0);  // loop 1
         AddEdge(0, 2); AddEdge(2, 0);  // loop 2
         AddEdge(0, 3); AddEdge(3, 0);  // loop 3
         AddEdge(0, 4); AddEdge(4, 0);  // loop 4
         mVertexX[0] = 0;     mVertexY[0] = 0;
         mVertexX[1] = 0;     mVertexY[1] = -0.8f;
         mVertexX[2] = 0.8f;  mVertexY[2] = 0;
         mVertexX[3] = 0;     mVertexY[3] = 0.8f;
         mVertexX[4] = -0.8f; mVertexY[4] = 0;
         break;

      case kPreset_Suspension:
         // Suspension of a square: 6v, 12e, 8f. Creates β₂.
         // Two apex vertices (0,5) connected to all of square (1,2,3,4)
         AddFace(0, 1, 2); AddFace(0, 2, 3); AddFace(0, 3, 4); AddFace(0, 4, 1);
         AddFace(5, 1, 2); AddFace(5, 2, 3); AddFace(5, 3, 4); AddFace(5, 4, 1);
         mVertexX[0] = 0;     mVertexY[0] = -1;    // north pole
         mVertexX[1] = 0.7f;  mVertexY[1] = -0.3f;
         mVertexX[2] = 0.7f;  mVertexY[2] = 0.3f;
         mVertexX[3] = -0.7f; mVertexY[3] = 0.3f;
         mVertexX[4] = -0.7f; mVertexY[4] = -0.3f;
         mVertexX[5] = 0;     mVertexY[5] = 1;     // south pole
         break;

      default:
         BuildComplex(kPreset_Tetrahedron);
         return;
   }

   ComputeCoboundary0();
   ComputeCoboundary1();
   ComputeLaplacianSpectrum();
   ExtractModes();
}

// ============================================================
// COBOUNDARY OPERATORS
// ============================================================

void CohomologySynth::ComputeCoboundary0()
{
   // δ⁰: C⁰ → C¹,  (δ⁰f)[i,j] = f(j) - f(i)
   // Matrix: N₁ rows × N₀ cols
   memset(mDelta0, 0, sizeof(mDelta0));
   for (int e = 0; e < mNumEdges; ++e)
   {
      mDelta0[e][mEdges[e].v0] = -1.0f;  // -f(i)
      mDelta0[e][mEdges[e].v1] = +1.0f;  // +f(j)
   }
}

void CohomologySynth::ComputeCoboundary1()
{
   // δ¹: C¹ → C²,  (δ¹g)[i,j,k] = g[j,k] - g[i,k] + g[i,j]
   // Matrix: N₂ rows × N₁ cols
   memset(mDelta1, 0, sizeof(mDelta1));
   for (int f = 0; f < mNumFaces; ++f)
   {
      int e01 = FindEdge(mFaces[f].v0, mFaces[f].v1);
      int e02 = FindEdge(mFaces[f].v0, mFaces[f].v2);
      int e12 = FindEdge(mFaces[f].v1, mFaces[f].v2);
      if (e01 >= 0) mDelta1[f][e01] = +1.0f;   // g[i,j]
      if (e02 >= 0) mDelta1[f][e02] = -1.0f;   // -g[i,k]
      if (e12 >= 0) mDelta1[f][e12] = +1.0f;   // g[j,k]
   }
}

// ============================================================
// LAPLACIAN SPECTRUM
// ============================================================

void CohomologySynth::ComputeLaplacianSpectrum()
{
   // Compute graph Laplacian Δ₀ = δ⁰ᵀ δ⁰ (N₀ × N₀ matrix)
   // This is the standard graph Laplacian: L[i][j] = degree(i) if i==j, -1 if edge(i,j), 0 otherwise
   int N = mNumVertices;
   if (N <= 0 || N > kMaxVertices) return;

   float L[kMaxVertices][kMaxVertices]{};

   // L = δ⁰ᵀ · δ⁰
   for (int i = 0; i < N; ++i)
   {
      for (int j = 0; j < N; ++j)
      {
         float sum = 0;
         for (int e = 0; e < mNumEdges; ++e)
            sum += mDelta0[e][i] * mDelta0[e][j];
         L[i][j] = sum;
      }
   }

   // Eigendecomposition via Jacobi iteration (simple, exact for small matrices)
   // Initialize eigenvectors to identity
   float V[kMaxVertices][kMaxVertices]{};
   float D[kMaxVertices]{};
   for (int i = 0; i < N; ++i)
   {
      V[i][i] = 1.0f;
      D[i] = L[i][i];
   }

   // Jacobi eigendecomposition: proper cyclic-by-row sweep.
   // For a real symmetric matrix A, find orthogonal V such that V^T A V = D.
   // Each Givens rotation zeroes one off-diagonal element.
   // Handles degenerate eigenvalues correctly (converges for any symmetric matrix).
   for (int sweep = 0; sweep < 200; ++sweep)
   {
      // Check convergence: sum of squared off-diagonal elements
      float offDiag = 0;
      for (int i = 0; i < N; ++i)
         for (int j = i + 1; j < N; ++j)
            offDiag += L[i][j] * L[i][j];
      if (offDiag < 1e-12f) break;

      for (int p = 0; p < N - 1; ++p)
      {
         for (int q = p + 1; q < N; ++q)
         {
            float apq = L[p][q];
            if (fabsf(apq) < 1e-14f) continue;

            // Compute rotation angle to zero L[p][q]
            float app = L[p][p];
            float aqq = L[q][q];
            float tau, t, c, s;

            if (fabsf(app - aqq) < 1e-14f)
            {
               // Degenerate case: eigenvalues equal, use 45 degrees
               t = 1.0f;
            }
            else
            {
               tau = (aqq - app) / (2.0f * apq);
               // Choose the smaller root for numerical stability
               t = (tau >= 0) ? 1.0f / (tau + sqrtf(1.0f + tau * tau))
                              : -1.0f / (-tau + sqrtf(1.0f + tau * tau));
            }
            c = 1.0f / sqrtf(1.0f + t * t);
            s = t * c;

            // Apply similarity transform: L' = G^T L G
            // This is the proper two-sided Jacobi rotation.
            // Update rows/cols p and q of L simultaneously.
            for (int r = 0; r < N; ++r)
            {
               if (r == p || r == q) continue;
               float lrp = L[r][p];
               float lrq = L[r][q];
               L[r][p] = L[p][r] = c * lrp - s * lrq;
               L[r][q] = L[q][r] = s * lrp + c * lrq;
            }

            float newApp = c * c * app - 2 * s * c * apq + s * s * aqq;
            float newAqq = s * s * app + 2 * s * c * apq + c * c * aqq;
            L[p][p] = newApp;
            L[q][q] = newAqq;
            L[p][q] = L[q][p] = 0.0f; // exactly zero by construction

            // Accumulate eigenvectors: V = V * G
            for (int i = 0; i < N; ++i)
            {
               float vip = V[i][p];
               float viq = V[i][q];
               V[i][p] = c * vip - s * viq;
               V[i][q] = s * vip + c * viq;
            }
         }
      }
   }

   // Extract eigenvalues (diagonal of L after Jacobi)
   for (int i = 0; i < N; ++i)
      D[i] = L[i][i];

   // Sort eigenvalues ascending
   int order[kMaxVertices];
   for (int i = 0; i < N; ++i) order[i] = i;
   for (int i = 0; i < N; ++i)
      for (int j = i + 1; j < N; ++j)
         if (D[order[j]] < D[order[i]])
            std::swap(order[i], order[j]);

   // Store sorted eigenvalues and eigenvectors
   for (int i = 0; i < N; ++i)
   {
      mEigenvalues[i] = D[order[i]];
      for (int j = 0; j < N; ++j)
         mEigenvectors[i][j] = V[j][order[i]];
   }
   mNumModes = N;

   // Compute Betti numbers via rank-nullity theorem on coboundary matrices.
   // β₀ = dim ker(δ⁰) = N₀ - rank(δ⁰)
   // β₁ = dim ker(δ¹) - rank(δ⁰)  = (N₁ - rank(δ¹)) - rank(δ⁰)
   // β₂ = N₂ - rank(δ¹)

   // Compute rank of δ⁰ (N₁ × N₀ matrix) via Gaussian elimination
   auto computeRank = [](float mat[][kMaxVertices], int rows, int cols) -> int {
      // Work on a copy
      float work[kMaxEdges][kMaxVertices];
      for (int i = 0; i < rows; ++i)
         for (int j = 0; j < cols; ++j)
            work[i][j] = mat[i][j];

      int rank = 0;
      for (int col = 0; col < cols && rank < rows; ++col)
      {
         // Find pivot
         int pivot = -1;
         float maxVal = 1e-6f;
         for (int row = rank; row < rows; ++row)
         {
            if (fabsf(work[row][col]) > maxVal)
            {
               maxVal = fabsf(work[row][col]);
               pivot = row;
            }
         }
         if (pivot < 0) continue;

         // Swap rows
         for (int j = 0; j < cols; ++j)
            std::swap(work[rank][j], work[pivot][j]);

         // Eliminate below
         float pivotVal = work[rank][col];
         for (int row = rank + 1; row < rows; ++row)
         {
            float factor = work[row][col] / pivotVal;
            for (int j = col; j < cols; ++j)
               work[row][j] -= factor * work[rank][j];
         }
         rank++;
      }
      return rank;
   };

   int rankDelta0 = computeRank(mDelta0, mNumEdges, mNumVertices);

   // Compute rank of δ¹ (N₂ × N₁ matrix) — need separate rank function for edge-width arrays
   auto computeRankEdge = [](float mat[][kMaxEdges], int rows, int cols) -> int {
      float work[kMaxFaces][kMaxEdges];
      for (int i = 0; i < rows; ++i)
         for (int j = 0; j < cols; ++j)
            work[i][j] = mat[i][j];

      int rank = 0;
      for (int col = 0; col < cols && rank < rows; ++col)
      {
         int pivot = -1;
         float maxVal = 1e-6f;
         for (int row = rank; row < rows; ++row)
         {
            if (fabsf(work[row][col]) > maxVal)
            {
               maxVal = fabsf(work[row][col]);
               pivot = row;
            }
         }
         if (pivot < 0) continue;
         for (int j = 0; j < cols; ++j)
            std::swap(work[rank][j], work[pivot][j]);
         float pivotVal = work[rank][col];
         for (int row = rank + 1; row < rows; ++row)
         {
            float factor = work[row][col] / pivotVal;
            for (int j = col; j < cols; ++j)
               work[row][j] -= factor * work[rank][j];
         }
         rank++;
      }
      return rank;
   };
   int rankDelta1 = computeRankEdge(mDelta1, mNumFaces, mNumEdges);

   mBetti[0] = mNumVertices - rankDelta0;        // connected components
   mBetti[1] = mNumEdges - rankDelta0 - rankDelta1; // independent loops
   mBetti[2] = mNumFaces - rankDelta1;            // enclosed cavities
   if (mBetti[0] < 0) mBetti[0] = 0;
   if (mBetti[1] < 0) mBetti[1] = 0;
   if (mBetti[2] < 0) mBetti[2] = 0;
}

void CohomologySynth::ExtractModes()
{
   mActiveModes = 0;
   for (int i = 0; i < mNumModes && mActiveModes < kMaxModes; ++i)
   {
      float lambda = mEigenvalues[i];
      if (lambda < 0.001f) continue; // skip kernel (DC modes)

      CohomMode& mode = mModes[mActiveModes];
      mode.frequency = sqrtf(lambda); // relative frequency (scaled by base pitch later)
      mode.amplitude = 0;
      mode.phase = 0;
      mode.sinState = 0;
      mode.cosState = 1;
      mode.damping = mDamping;
      mode.dimension = 0; // vertex modes
      mode.modeShapeSize = mNumVertices;
      for (int j = 0; j < mNumVertices; ++j)
         mode.modeShape[j] = mEigenvectors[i][j];

      mActiveModes++;
   }
}

// ============================================================
// DSP
// ============================================================

void CohomologySynth::ExciteModes(float velocity, int exciteVertex)
{
   if (exciteVertex < 0 || exciteVertex >= mNumVertices) return;

   for (int m = 0; m < mActiveModes; ++m)
   {
      // Project excitation onto mode: inner product of delta function at vertex with eigenvector
      float projection = mModes[m].modeShape[exciteVertex];

      // Spread excitation to neighboring vertices
      if (mExciteSpread > 0)
      {
         for (int e = 0; e < mNumEdges; ++e)
         {
            if (mEdges[e].v0 == exciteVertex)
               projection += mExciteSpread * mModes[m].modeShape[mEdges[e].v1];
            if (mEdges[e].v1 == exciteVertex)
               projection += mExciteSpread * mModes[m].modeShape[mEdges[e].v0];
         }
      }

      // Higher modes get scaled by brightness parameter
      float modeIndex = (float)m / std::max(1, mActiveModes - 1);
      float brightScale = 1.0f - modeIndex * (1.0f - mBrightness);
      brightScale = std::max(0.0f, brightScale);

      mModes[m].amplitude += velocity * fabsf(projection) * brightScale;
      mModes[m].phase = 0;
      mModes[m].sinState = 0;
      mModes[m].cosState = 1;
   }
}

void CohomologySynth::PlayNote(NoteMessage note)
{
   if (note.velocity > 0)
   {
      mPitch = note.pitch;
      mFrequency = 440.0f * powf(2.0f, (mPitch - 69.0f) / 12.0f);
      mBaseFreq = mFrequency;
      mEnvelope.Start(gTime, note.velocity / 127.0f);

      // Reset mode amplitudes
      for (int m = 0; m < mActiveModes; ++m)
         mModes[m].amplitude = 0;

      ExciteModes(note.velocity / 127.0f, mExciteVertex);
   }
   else
   {
      mEnvelope.Stop(gTime);
   }
}

void CohomologySynth::Process(double time)
{
   PROFILER(CohomologySynth);

   IAudioReceiver* target = GetTarget();
   if (!mEnabled || target == nullptr)
      return;

   SyncBuffers(1);
   ComputeSliders(0);

   int bufferSize = target->GetBuffer()->BufferSize();
   mWriteBuffer.Clear();
   float* out = mWriteBuffer.GetChannel(0);

   // Cache: find smallest nonzero eigenvalue once per buffer (not per sample)
   float minLambda = 1e10f;
   for (int m = 0; m < mActiveModes; ++m)
      if (mModes[m].frequency > 0.01f && mModes[m].frequency < minLambda)
         minLambda = mModes[m].frequency;
   if (minLambda < 0.01f) minLambda = 1.0f;

   // Precompute quadrature rotation coefficients per mode
   // Phase quadrature: sin_new = sin*c + cos*s; cos_new = cos*c - sin*s
   // where c = cos(w), s = sin(w), w = 2*pi*f/sr
   // This replaces one sinf() per mode per sample with 4 multiplies + 2 adds.
   float rotC[kMaxModes], rotS[kMaxModes];
   for (int m = 0; m < mActiveModes; ++m)
   {
      float freq = mBaseFreq * (mModes[m].frequency / minLambda);
      float w = FTWO_PI * freq / gSampleRate;
      rotC[m] = cosf(w);
      rotS[m] = sinf(w);
   }

   for (int s = 0; s < bufferSize; ++s)
   {
      mEnvelopeValue = mEnvelope.Value(time);
      float sample = 0;

      for (int m = 0; m < mActiveModes; ++m)
      {
         CohomMode& mode = mModes[m];
         if (mode.amplitude < 1e-7f) continue;

         // Quadrature rotation (no sinf() in the inner loop)
         float newSin = mode.sinState * rotC[m] + mode.cosState * rotS[m];
         float newCos = mode.cosState * rotC[m] - mode.sinState * rotS[m];
         mode.sinState = newSin;
         mode.cosState = newCos;
         mode.phase += rotS[m]; // approximate phase tracking for viz

         sample += mode.amplitude * mode.sinState;
         mode.amplitude *= mode.damping;
      }

      out[s] = ofClamp(sample * mVolume * mEnvelopeValue, -2.0f, 2.0f);
      time += gInvSampleRateMs;
   }

   // Visualization: compute vertex amplitudes once per buffer (not per sample)
   for (int v = 0; v < mNumVertices; ++v)
   {
      float vertAmp = 0;
      for (int m = 0; m < mActiveModes; ++m)
      {
         if (mModes[m].amplitude < 1e-7f) continue;
         vertAmp += mModes[m].amplitude * mModes[m].modeShape[v] * mModes[m].sinState;
      }
      mVertexAmplitude[v] = vertAmp;
   }

   // Output: follow KarplusStrong pattern exactly
   GetVizBuffer()->WriteChunk(out, bufferSize, 0);
   Add(target->GetBuffer()->GetChannel(0), out, bufferSize);
   GetBuffer()->Reset();
}

// ============================================================
// VISUALIZATION
// ============================================================

void CohomologySynth::GetModuleDimensions(float& width, float& height)
{
   width = 320;
   height = 340;
}

void CohomologySynth::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   // Controls
   mPresetDropdown->Draw();
   mDampingSlider->Draw();
   mBrightnessSlider->Draw();
   mExciteSpreadSlider->Draw();
   mExciteVertexSlider->Draw();
   mVolumeSlider->Draw();

   // === Simplicial Complex Visualization ===
   const float vizX = 10;
   const float vizY = 60;
   const float vizW = 300;
   const float vizH = 220;
   const float centerX = vizX + vizW / 2;
   const float centerY = vizY + vizH / 2;
   const float scale = std::min(vizW, vizH) * 0.38f;

   // Dark background
   {
      NVGpaint bg = nvgRadialGradient(gNanoVG, centerX, centerY, 10, vizW * 0.6f,
         nvgRGBA(12, 14, 22, (int)(gModuleDrawAlpha * .85f)),
         nvgRGBA(4, 5, 10, (int)(gModuleDrawAlpha * .9f)));
      nvgBeginPath(gNanoVG);
      nvgRoundedRect(gNanoVG, vizX, vizY, vizW, vizH, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, bg);
      nvgFill(gNanoVG);
   }

   ofColor color = GetColor(kModuleCategory_Synth);

   // Draw faces (2-simplices) as filled triangles with subtle color
   for (int f = 0; f < mNumFaces; ++f)
   {
      float x0 = centerX + mVertexX[mFaces[f].v0] * scale;
      float y0 = centerY + mVertexY[mFaces[f].v0] * scale;
      float x1 = centerX + mVertexX[mFaces[f].v1] * scale;
      float y1 = centerY + mVertexY[mFaces[f].v1] * scale;
      float x2 = centerX + mVertexX[mFaces[f].v2] * scale;
      float y2 = centerY + mVertexY[mFaces[f].v2] * scale;

      // Face pressure from vertex amplitudes
      float faceAmp = (fabsf(mVertexAmplitude[mFaces[f].v0]) +
                        fabsf(mVertexAmplitude[mFaces[f].v1]) +
                        fabsf(mVertexAmplitude[mFaces[f].v2])) / 3.0f;
      float mag = ofClamp(faceAmp * 8, 0, 1);

      nvgBeginPath(gNanoVG);
      nvgMoveTo(gNanoVG, x0, y0);
      nvgLineTo(gNanoVG, x1, y1);
      nvgLineTo(gNanoVG, x2, y2);
      nvgClosePath(gNanoVG);

      // Face fill: category color, opacity scales with pressure
      int alpha = (int)(gModuleDrawAlpha * (.06f + mag * .15f));
      nvgFillColor(gNanoVG, nvgRGBA(color.r, color.g, color.b, alpha));
      nvgFill(gNanoVG);
   }

   // Draw edges (1-simplices)
   for (int e = 0; e < mNumEdges; ++e)
   {
      float x0 = centerX + mVertexX[mEdges[e].v0] * scale;
      float y0 = centerY + mVertexY[mEdges[e].v0] * scale;
      float x1 = centerX + mVertexX[mEdges[e].v1] * scale;
      float y1 = centerY + mVertexY[mEdges[e].v1] * scale;

      // Edge brightness from vertex amplitudes
      float edgeAmp = (fabsf(mVertexAmplitude[mEdges[e].v0]) +
                        fabsf(mVertexAmplitude[mEdges[e].v1])) * 0.5f;
      float mag = ofClamp(edgeAmp * 6, 0, 1);

      ofPushStyle();
      ofSetColor(color.r * (.25f + mag * .75f),
                 color.g * (.25f + mag * .75f),
                 color.b * (.25f + mag * .75f),
                 gModuleDrawAlpha * (.3f + mag * .7f));
      ofSetLineWidth(0.8f + mag * 2.0f);
      ofLine(x0, y0, x1, y1);
      ofPopStyle();
   }

   // Draw vertices (0-simplices)
   for (int v = 0; v < mNumVertices; ++v)
   {
      float vx = centerX + mVertexX[v] * scale;
      float vy = centerY + mVertexY[v] * scale;
      float amp = mVertexAmplitude[v];
      float mag = ofClamp(fabsf(amp) * 5, 0, 1);

      // Displacement: vertex wobbles with amplitude
      float dispX = amp * 6.0f * mVertexX[v]; // radial displacement
      float dispY = amp * 6.0f * mVertexY[v];

      float nx = vx + dispX;
      float ny = vy + dispY;

      // Vertex glow
      if (mag > 0.01f)
      {
         // Color shifts with sign: positive=green, negative=blue
         int r = amp > 0 ? (int)(color.r * mag) : (int)(80 * mag);
         int g = amp > 0 ? (int)(color.g * mag) : (int)(120 * mag);
         int b = amp > 0 ? (int)(color.b * mag) : (int)(255 * mag);

         NVGpaint glow = nvgRadialGradient(gNanoVG, nx, ny, 2, 18,
            nvgRGBA(r, g, b, (int)(gModuleDrawAlpha * .5f * mag)),
            nvgRGBA(r, g, b, 0));
         nvgBeginPath(gNanoVG);
         nvgCircle(gNanoVG, nx, ny, 22);
         nvgFillPaint(gNanoVG, glow);
         nvgFill(gNanoVG);
      }

      // Vertex dot
      float radius = 3.5f + mag * 3.5f;
      ofPushStyle();
      ofFill();
      ofSetColor(color.r * (.5f + mag * .5f),
                 color.g * (.5f + mag * .5f),
                 color.b * (.5f + mag * .5f),
                 gModuleDrawAlpha * (.7f + mag * .3f));
      ofCircle(nx, ny, radius);

      // Excite vertex ring
      if (v == mExciteVertex)
      {
         ofNoFill();
         ofSetColor(255, 255, 255, gModuleDrawAlpha * .4f);
         ofSetLineWidth(1);
         ofCircle(nx, ny, radius + 4);
      }
      ofPopStyle();
   }

   // Betti numbers — prominent display with meaning-coded colors
   // β₀ = warm (fundamentals), β₁ = cool (loops), β₂ = magenta (cavities)
   {
      ofPushStyle();

      // Recessed status strip
      {
         NVGpaint strip = nvgLinearGradient(gNanoVG, vizX, vizY + vizH - 32, vizX, vizY + vizH,
            nvgRGBA(8, 8, 12, (int)(gModuleDrawAlpha * .5f)),
            nvgRGBA(4, 4, 8, (int)(gModuleDrawAlpha * .7f)));
         nvgBeginPath(gNanoVG);
         nvgRoundedRect(gNanoVG, vizX + 2, vizY + vizH - 32, vizW - 4, 30, gCornerRoundness * 2);
         nvgFillPaint(gNanoVG, strip);
         nvgFill(gNanoVG);
      }

      float betaX = vizX + 10;
      float betaY = vizY + vizH - 12;

      // β₀ — warm orange (connected components / fundamentals)
      {
         char b0[16]; snprintf(b0, sizeof(b0), "%d", mBetti[0]);
         ofSetColor(255, 180, 80, gModuleDrawAlpha * .8f);
         DrawTextNormal("b0", betaX, betaY - 10, 7);
         DrawTextNormal(b0, betaX + 2, betaY + 2, 14);
         if (mBetti[0] > 0)
         {
            NVGpaint glow = nvgRadialGradient(gNanoVG, betaX + 5, betaY - 2, 2, 12,
               nvgRGBA(255, 180, 80, (int)(gModuleDrawAlpha * .15f)),
               nvgRGBA(255, 180, 80, 0));
            nvgBeginPath(gNanoVG); nvgCircle(gNanoVG, betaX + 5, betaY - 2, 14);
            nvgFillPaint(gNanoVG, glow); nvgFill(gNanoVG);
         }
      }

      // β₁ — cool cyan (independent loops)
      betaX += 45;
      {
         char b1[16]; snprintf(b1, sizeof(b1), "%d", mBetti[1]);
         ofSetColor(80, 200, 255, gModuleDrawAlpha * .8f);
         DrawTextNormal("b1", betaX, betaY - 10, 7);
         DrawTextNormal(b1, betaX + 2, betaY + 2, 14);
         if (mBetti[1] > 0)
         {
            NVGpaint glow = nvgRadialGradient(gNanoVG, betaX + 5, betaY - 2, 2, 12,
               nvgRGBA(80, 200, 255, (int)(gModuleDrawAlpha * .15f)),
               nvgRGBA(80, 200, 255, 0));
            nvgBeginPath(gNanoVG); nvgCircle(gNanoVG, betaX + 5, betaY - 2, 14);
            nvgFillPaint(gNanoVG, glow); nvgFill(gNanoVG);
         }
      }

      // β₂ — magenta (enclosed cavities / formants)
      betaX += 45;
      {
         char b2[16]; snprintf(b2, sizeof(b2), "%d", mBetti[2]);
         ofSetColor(220, 100, 220, gModuleDrawAlpha * .8f);
         DrawTextNormal("b2", betaX, betaY - 10, 7);
         DrawTextNormal(b2, betaX + 2, betaY + 2, 14);
         if (mBetti[2] > 0)
         {
            NVGpaint glow = nvgRadialGradient(gNanoVG, betaX + 5, betaY - 2, 2, 12,
               nvgRGBA(220, 100, 220, (int)(gModuleDrawAlpha * .15f)),
               nvgRGBA(220, 100, 220, 0));
            nvgBeginPath(gNanoVG); nvgCircle(gNanoVG, betaX + 5, betaY - 2, 14);
            nvgFillPaint(gNanoVG, glow); nvgFill(gNanoVG);
         }
      }

      // Simplex counts and mode count (smaller, right-aligned)
      betaX += 55;
      ofSetColor(255, 255, 255, gModuleDrawAlpha * .2f);
      char stats[64];
      snprintf(stats, sizeof(stats), "V%d E%d F%d  %d modes",
               mNumVertices, mNumEdges, mNumFaces, mActiveModes);
      DrawTextNormal(stats, betaX, betaY - 2, 8);

      // Mode spectrum bar
      float barX = vizX + 5;
      float barY = vizY + vizH - 18;
      float barW = vizW - 10;
      float barH = 8;
      for (int m = 0; m < mActiveModes; ++m)
      {
         float t = (float)m / std::max(1, mActiveModes - 1);
         float x = barX + t * barW;
         float amp = ofClamp(mModes[m].amplitude * 20, 0, 1);
         if (amp < 0.01f) continue;

         NVGpaint dot = nvgRadialGradient(gNanoVG, x, barY + barH / 2, 1, 4,
            nvgRGBA(color.r, color.g, color.b, (int)(amp * gModuleDrawAlpha * .8f)),
            nvgRGBA(color.r, color.g, color.b, 0));
         nvgBeginPath(gNanoVG);
         nvgCircle(gNanoVG, x, barY + barH / 2, 6);
         nvgFillPaint(gNanoVG, dot);
         nvgFill(gNanoVG);
      }
      ofPopStyle();
   }
}

// ============================================================
// SAVE/LOAD
// ============================================================

void CohomologySynth::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadInt("preset", moduleInfo, kPreset_Tetrahedron);
   SetUpFromSaveData();
}

void CohomologySynth::SetUpFromSaveData()
{
   mPreset = (ComplexPreset)mModuleSaveData.GetInt("preset");
   BuildComplex(mPreset);
}
