/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/

#include "CohomologyVerb.h"
#include "OpenFrameworksPort.h"
#include "SynthGlobals.h"
#include "IAudioReceiver.h"
#include "ModularSynth.h"
#include "Profiler.h"
#include "ofxJSONElement.h"
#include "UIControlMacros.h"
#include "nanovg/nanovg.h"
#include <cmath>
#include <cstring>

// Prime-ish delay lengths in samples at 48kHz for different topologies
// Each row: delays for one preset, scaled by mSize at runtime
static const int kBaseDelays[][kVerbMaxDelays] = {
   { 1117, 1553, 1867, 0, 0, 0, 0, 0 },           // Room (3 delays)
   { 1087, 1423, 1777, 2131, 0, 0, 0, 0 },         // Hall (4 delays)
   { 887, 1151, 1423, 1699, 1951, 2203, 0, 0 },    // Chamber (6 delays)
   { 743, 997, 1249, 1511, 1747, 2003, 2251, 2503 },// Cathedral (8 delays)
   { 953, 1301, 1657, 2011, 2347, 2689, 3001, 0 },  // Cave (7 delays)
};

// Number of delay lines per preset
static const int kNumDelaysPerPreset[] = { 3, 4, 6, 8, 7 };

CohomologyVerb::CohomologyVerb()
: IAudioProcessor(gBufferSize)
{
   BuildFDN(kVerb_Hall);
}

CohomologyVerb::~CohomologyVerb()
{
}

void CohomologyVerb::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   UIBLOCK0();
   DROPDOWN(mPresetDropdown, "space", (int*)&mPreset, 80);
   FLOATSLIDER(mSizeSlider, "size", &mSize, 0.1f, 1.5f);
   FLOATSLIDER(mDecaySlider, "decay", &mDecay, 0.1f, 0.99f);
   FLOATSLIDER(mDampingSlider, "damp", &mDamping, 0.1f, 0.99f);
   FLOATSLIDER(mPreDelaySlider, "pre", &mPreDelay, 0, 0.1f);
   FLOATSLIDER(mWetDrySlider, "mix", &mWetDry, 0, 1);
   ENDUIBLOCK0();

   mPresetDropdown->AddLabel("room", kVerb_Room);
   mPresetDropdown->AddLabel("hall", kVerb_Hall);
   mPresetDropdown->AddLabel("chamber", kVerb_Chamber);
   mPresetDropdown->AddLabel("cathedral", kVerb_Cathedral);
   mPresetDropdown->AddLabel("cave", kVerb_Cave);
}

void CohomologyVerb::DropdownUpdated(DropdownList* list, int oldVal, double time)
{
   if (list == mPresetDropdown)
      BuildFDN(mPreset);
}

void CohomologyVerb::BuildFDN(VerbPreset preset)
{
   int idx = (int)preset;
   if (idx < 0 || idx > 4) idx = 1;

   mNumDelays = kNumDelaysPerPreset[idx];

   // Set delay lengths (scaled by size)
   for (int i = 0; i < mNumDelays; ++i)
   {
      mDelayLens[i] = std::max(1, std::min((int)(kBaseDelays[idx][i] * mSize), kVerbMaxDelaySamples - 1));
      mWritePos[i] = 0;
      mDampState[i] = 0;
   }

   // Build mixing matrix from topology
   // The matrix should be orthogonal (energy-preserving) and well-diffused.
   // We use a Householder reflection: H = I - 2/N * ones(N,N)
   // This is the simplest unitary FDN matrix: all outputs get mixed equally
   // with sign inversion scaled by 2/N.
   memset(mMixMatrix, 0, sizeof(mMixMatrix));
   float N = (float)mNumDelays;
   for (int i = 0; i < mNumDelays; ++i)
   {
      for (int j = 0; j < mNumDelays; ++j)
      {
         if (i == j)
            mMixMatrix[i][j] = 1.0f - 2.0f / N;
         else
            mMixMatrix[i][j] = -2.0f / N;
      }
   }

   // Set Betti numbers based on preset topology
   // These are the real Betti numbers of the underlying complex
   switch (preset)
   {
      case kVerb_Room:      mBetti[0]=1; mBetti[1]=0; mBetti[2]=0; break; // triangle
      case kVerb_Hall:      mBetti[0]=1; mBetti[1]=0; mBetti[2]=1; break; // tetrahedron
      case kVerb_Chamber:   mBetti[0]=1; mBetti[1]=0; mBetti[2]=1; break; // octahedron
      case kVerb_Cathedral: mBetti[0]=1; mBetti[1]=4; mBetti[2]=0; break; // bouquet
      case kVerb_Cave:      mBetti[0]=1; mBetti[1]=2; mBetti[2]=1; break; // torus
   }

   // Vertex positions for visualization
   float cx = 0.5f, cy = 0.5f;
   for (int i = 0; i < mNumDelays; ++i)
   {
      float angle = (float)i / mNumDelays * FTWO_PI - FPI / 2;
      mVertX[i] = cx + cosf(angle) * 0.38f;
      mVertY[i] = cy + sinf(angle) * 0.38f;
   }

   // Clear delay buffers
   memset(mDelayBufs, 0, sizeof(mDelayBufs));
   memset(mPreDelayBuf, 0, sizeof(mPreDelayBuf));
   mPreDelayWritePos = 0;
}

void CohomologyVerb::Process(double time)
{
   PROFILER(CohomologyVerb);

   IAudioReceiver* target = GetTarget();
   if (!mEnabled || target == nullptr)
      return;

   SyncBuffers();
   ComputeSliders(0);

   int bufferSize = GetBuffer()->BufferSize();
   float* input = GetBuffer()->GetChannel(0);

   int preDelaySamples = std::max(1, std::min((int)(mPreDelay * gSampleRate), kVerbMaxDelaySamples - 1));

   // Update delay lengths from size parameter
   int idx = (int)mPreset;
   if (idx < 0 || idx > 4) idx = 1;
   for (int i = 0; i < mNumDelays; ++i)
      mDelayLens[i] = std::max(1, std::min((int)(kBaseDelays[idx][i] * mSize), kVerbMaxDelaySamples - 1));

   for (int s = 0; s < bufferSize; ++s)
   {
      float dry = input[s];

      // Pre-delay
      mPreDelayBuf[mPreDelayWritePos] = dry;
      int preReadPos = (mPreDelayWritePos - preDelaySamples + kVerbMaxDelaySamples) % kVerbMaxDelaySamples;
      float preDelayed = mPreDelayBuf[preReadPos];
      mPreDelayWritePos = (mPreDelayWritePos + 1) % kVerbMaxDelaySamples;

      // Read from all delay lines
      float delayOuts[kVerbMaxDelays];
      for (int i = 0; i < mNumDelays; ++i)
      {
         int readPos = (mWritePos[i] - mDelayLens[i] + kVerbMaxDelaySamples) % kVerbMaxDelaySamples;
         delayOuts[i] = mDelayBufs[i][readPos];
      }

      // Apply mixing matrix
      float mixed[kVerbMaxDelays];
      for (int i = 0; i < mNumDelays; ++i)
      {
         mixed[i] = 0;
         for (int j = 0; j < mNumDelays; ++j)
            mixed[i] += mMixMatrix[i][j] * delayOuts[j];
      }

      // Apply decay and HF damping, write back + inject input
      float wet = 0;
      for (int i = 0; i < mNumDelays; ++i)
      {
         // One-pole lowpass for HF damping: y = (1-d)*x + d*y_prev
         float damped = (1.0f - mDamping) * mixed[i] + mDamping * mDampState[i];
         mDampState[i] = damped;

         // Write: decayed feedback + input injection (distributed across all lines)
         mDelayBufs[i][mWritePos[i]] = damped * mDecay + preDelayed / mNumDelays;
         mWritePos[i] = (mWritePos[i] + 1) % kVerbMaxDelaySamples;

         wet += delayOuts[i];
      }
      wet /= mNumDelays;

      input[s] = ofClamp(dry * (1.0f - mWetDry) + wet * mWetDry, -4.0f, 4.0f);
   }

   // Viz: energy per delay
   for (int i = 0; i < mNumDelays; ++i)
   {
      int readPos = (mWritePos[i] - 1 + kVerbMaxDelaySamples) % kVerbMaxDelaySamples;
      mDelayViz[i] = mDelayBufs[i][readPos];
   }

   for (int ch = 0; ch < GetBuffer()->NumActiveChannels(); ++ch)
   {
      Add(target->GetBuffer()->GetChannel(ch), GetBuffer()->GetChannel(ch), bufferSize);
      GetVizBuffer()->WriteChunk(GetBuffer()->GetChannel(ch), bufferSize, ch);
   }
   GetBuffer()->Reset();
}

void CohomologyVerb::GetModuleDimensions(float& width, float& height)
{
   width = 220;
   height = 250;
}

void CohomologyVerb::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   mPresetDropdown->Draw();
   mSizeSlider->Draw();
   mDecaySlider->Draw();
   mDampingSlider->Draw();
   mPreDelaySlider->Draw();
   mWetDrySlider->Draw();

   // Reverb topology visualization
   float vizX = 10, vizY = 110, vizW = 200, vizH = 130;
   float cx = vizX + vizW / 2, cy = vizY + vizH / 2;

   {
      NVGpaint bg = nvgRadialGradient(gNanoVG, cx, cy, 5, vizW * 0.5f,
         nvgRGBA(10, 10, 18, (int)(gModuleDrawAlpha * .8f)),
         nvgRGBA(3, 3, 8, (int)(gModuleDrawAlpha * .9f)));
      nvgBeginPath(gNanoVG);
      nvgRoundedRect(gNanoVG, vizX, vizY, vizW, vizH, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, bg);
      nvgFill(gNanoVG);
   }

   ofColor color = GetColor(kModuleCategory_Audio);

   // Draw edges (all-to-all connections from the Householder matrix)
   for (int i = 0; i < mNumDelays; ++i)
   {
      for (int j = i + 1; j < mNumDelays; ++j)
      {
         float nx0 = vizX + mVertX[i] * vizW;
         float ny0 = vizY + mVertY[i] * vizH;
         float nx1 = vizX + mVertX[j] * vizW;
         float ny1 = vizY + mVertY[j] * vizH;

         float e = (fabsf(mDelayViz[i]) + fabsf(mDelayViz[j])) * 3;
         float m = ofClamp(e, 0, 1);

         ofSetColor(color.r * (.1f + m * .4f), color.g * (.1f + m * .4f), color.b * (.1f + m * .4f),
                    gModuleDrawAlpha * (.1f + m * .3f));
         ofSetLineWidth(0.5f + m);
         ofLine(nx0, ny0, nx1, ny1);
      }
   }

   // Draw nodes (delay lines)
   for (int i = 0; i < mNumDelays; ++i)
   {
      float nx = vizX + mVertX[i] * vizW;
      float ny = vizY + mVertY[i] * vizH;
      float m = ofClamp(fabsf(mDelayViz[i]) * 5, 0, 1);

      if (m > 0.01f)
      {
         NVGpaint glow = nvgRadialGradient(gNanoVG, nx, ny, 2, 14,
            nvgRGBA(color.r, color.g, color.b, (int)(m * gModuleDrawAlpha * .35f)),
            nvgRGBA(color.r, color.g, color.b, 0));
         nvgBeginPath(gNanoVG); nvgCircle(gNanoVG, nx, ny, 16);
         nvgFillPaint(gNanoVG, glow); nvgFill(gNanoVG);
      }

      ofFill();
      ofSetColor(color.r * (.3f + m * .7f), color.g * (.3f + m * .7f), color.b * (.3f + m * .7f),
                 gModuleDrawAlpha * (.5f + m * .5f));
      ofCircle(nx, ny, 3 + m * 3);
   }

   // Betti numbers (small, bottom-right)
   {
      ofSetColor(255, 180, 80, gModuleDrawAlpha * .5f);
      char b[32]; snprintf(b, sizeof(b), "b(%d,%d,%d)", mBetti[0], mBetti[1], mBetti[2]);
      DrawTextNormal(b, vizX + 5, vizY + vizH - 5, 8);

      // Preset name
      const char* names[] = { "room", "hall", "chamber", "cathedral", "cave" };
      ofSetColor(255, 255, 255, gModuleDrawAlpha * .15f);
      DrawTextNormal(names[(int)mPreset], vizX + vizW - 50, vizY + vizH - 5, 8);
   }
}

void CohomologyVerb::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadString("target", moduleInfo);
   mModuleSaveData.LoadInt("preset", moduleInfo, kVerb_Hall);
   SetUpFromSaveData();
}

void CohomologyVerb::SetUpFromSaveData()
{
   SetTarget(TheSynth->FindModule(mModuleSaveData.GetString("target")));
   mPreset = (VerbPreset)mModuleSaveData.GetInt("preset");
   BuildFDN(mPreset);
}
