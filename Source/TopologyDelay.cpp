/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/

#include "TopologyDelay.h"
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

TopologyDelay::TopologyDelay()
: IAudioProcessor(gBufferSize)
{
   // Set up small ring lattice in the feedback path
   for (int i = 0; i < kDelayLatticeNodes; ++i)
      mNodes[i].len = 64 + i * 17; // prime-ish lengths for diffusion
}

TopologyDelay::~TopologyDelay()
{
}

void TopologyDelay::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   UIBLOCK0();
   FLOATSLIDER(mDelayTimeSlider, "time", &mDelayTime, 0.01f, 2.0f);
   FLOATSLIDER(mFeedbackSlider, "feedback", &mFeedback, 0, 0.95f);
   FLOATSLIDER(mDiffusionSlider, "diffuse", &mDiffusion, 0, 0.8f);
   FLOATSLIDER_DIGITS(mDampingSlider, "damp", &mDamping, 0.98f, 0.999f, 3);
   FLOATSLIDER(mWetDrySlider, "mix", &mWetDry, 0, 1);
   ENDUIBLOCK0();
}

void TopologyDelay::Process(double time)
{
   PROFILER(TopologyDelay);

   IAudioReceiver* target = GetTarget();
   if (!mEnabled || target == nullptr)
      return;

   SyncBuffers();
   ComputeSliders(0);

   int bufferSize = GetBuffer()->BufferSize();
   float* input = GetBuffer()->GetChannel(0);

   int delaySamples = std::max(1, std::min((int)(mDelayTime * gSampleRate), kDelayMaxSamples - 1));

   for (int s = 0; s < bufferSize; ++s)
   {
      float dry = input[s];

      // Read from main delay line
      int readPos = (mDelayWritePos - delaySamples + kDelayMaxSamples) % kDelayMaxSamples;
      float delayed = mDelayBuf[readPos];

      // Pass delayed signal through the lattice for diffusion
      // Inject into node 0
      mNodes[0].fwd += delayed * mDiffusion;

      // Scatter and propagate (small ring of 4 nodes)
      float fSnap[kDelayLatticeNodes], bSnap[kDelayLatticeNodes];
      for (int i = 0; i < kDelayLatticeNodes; ++i)
      {
         // Simple scattering: average neighbors
         float f = mNodes[i].fwd, b = mNodes[i].bwd;
         mNodes[i].fwd = (f * 0.7f + b * 0.3f) * mDamping;
         mNodes[i].bwd = (b * 0.7f + f * 0.3f) * mDamping;
         fSnap[i] = mNodes[i].fwd;
         bSnap[i] = mNodes[i].bwd;
      }

      // Propagate ring: edge i carries forward (i→i+1) and backward (i+1→i)
      for (int i = 0; i < kDelayLatticeNodes; ++i)
      {
         int next = (i + 1) % kDelayLatticeNodes;
         auto& edge = mNodes[i]; // edge buffer owned by node i

         // Forward: i → delay → i+1
         edge.buf[edge.writePos] = fSnap[i];
         int ridx = (edge.writePos - (edge.len - 1) + edge.len * 2) % edge.len;
         mNodes[next].fwd = edge.buf[ridx];

         // Backward: i+1 → same edge delay → i (same length, no mismatch)
         edge.bufBack[edge.writePos] = bSnap[next];
         mNodes[i].bwd = edge.bufBack[ridx]; // same index, same buffer length

         edge.writePos = (edge.writePos + 1) % edge.len;
      }

      // Collect diffused output from all nodes
      float diffused = 0;
      for (int i = 0; i < kDelayLatticeNodes; ++i)
         diffused += mNodes[i].fwd + mNodes[i].bwd;
      diffused /= kDelayLatticeNodes * 2;

      // Blend: direct delayed + lattice-diffused
      float wet = delayed * (1.0f - mDiffusion) + diffused;

      // Write feedback into delay line
      mDelayBuf[mDelayWritePos] = dry + wet * mFeedback;
      mDelayWritePos = (mDelayWritePos + 1) % kDelayMaxSamples;

      // Output
      input[s] = ofClamp(dry * (1.0f - mWetDry) + wet * mWetDry, -1.0f, 1.0f); if (!std::isfinite(input[s])) input[s] = 0;
   }

   // Viz
   for (int i = 0; i < kDelayLatticeNodes; ++i)
      mNodeViz[i] = mNodes[i].fwd + mNodes[i].bwd;

   for (int ch = 0; ch < GetBuffer()->NumActiveChannels(); ++ch)
   {
      Add(target->GetBuffer()->GetChannel(ch), GetBuffer()->GetChannel(ch), bufferSize);
      GetVizBuffer()->WriteChunk(GetBuffer()->GetChannel(ch), bufferSize, ch);
   }
   GetBuffer()->Reset();
}

void TopologyDelay::GetModuleDimensions(float& width, float& height)
{
   width = 180;
   height = 190;
}

void TopologyDelay::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   mDelayTimeSlider->Draw();
   mFeedbackSlider->Draw();
   mDiffusionSlider->Draw();
   mDampingSlider->Draw();
   mWetDrySlider->Draw();

   // Small lattice ring viz
   float vizX = 10, vizY = 95, vizW = 160, vizH = 85;
   float cx = vizX + vizW / 2, cy = vizY + vizH / 2;

   {
      NVGpaint bg = nvgRadialGradient(gNanoVG, cx, cy, 5, vizW * 0.45f,
         nvgRGBA(10, 12, 20, (int)(gModuleDrawAlpha * .8f)),
         nvgRGBA(4, 5, 10, (int)(gModuleDrawAlpha * .85f)));
      nvgBeginPath(gNanoVG);
      nvgRoundedRect(gNanoVG, vizX, vizY, vizW, vizH, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, bg);
      nvgFill(gNanoVG);
   }

   ofColor color = GetColor(kModuleCategory_Audio);
   float rad = std::min(vizW, vizH) * 0.3f;

   for (int i = 0; i < kDelayLatticeNodes; ++i)
   {
      float angle = (float)i / kDelayLatticeNodes * FTWO_PI - FPI / 2;
      float nx = cx + cosf(angle) * rad;
      float ny = cy + sinf(angle) * rad;
      int j = (i + 1) % kDelayLatticeNodes;
      float ax = cx + cosf((float)j / kDelayLatticeNodes * FTWO_PI - FPI / 2) * rad;
      float ay = cy + sinf((float)j / kDelayLatticeNodes * FTWO_PI - FPI / 2) * rad;

      float m = ofClamp(fabsf(mNodeViz[i]) * 5, 0, 1);

      // Edge
      ofSetColor(color.r * (.2f + m * .5f), color.g * (.2f + m * .5f), color.b * (.2f + m * .5f),
                 gModuleDrawAlpha * (.2f + m * .5f));
      ofSetLineWidth(.8f + m * 1.5f);
      ofLine(nx, ny, ax, ay);

      // Node
      if (m > 0.01f)
      {
         NVGpaint glow = nvgRadialGradient(gNanoVG, nx, ny, 2, 10,
            nvgRGBA(color.r, color.g, color.b, (int)(m * gModuleDrawAlpha * .4f)),
            nvgRGBA(color.r, color.g, color.b, 0));
         nvgBeginPath(gNanoVG); nvgCircle(gNanoVG, nx, ny, 12);
         nvgFillPaint(gNanoVG, glow); nvgFill(gNanoVG);
      }
      ofFill();
      ofSetColor(color.r * (.4f + m * .6f), color.g * (.4f + m * .6f), color.b * (.4f + m * .6f),
                 gModuleDrawAlpha * (.6f + m * .4f));
      ofCircle(nx, ny, 3 + m * 2);
   }

   ofSetColor(255, 255, 255, gModuleDrawAlpha * .15f);
   DrawTextNormal("topology delay", vizX + 5, vizY + vizH - 4, 8);
}

void TopologyDelay::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadString("target", moduleInfo);
   SetUpFromSaveData();
}

void TopologyDelay::SetUpFromSaveData()
{
   SetTarget(TheSynth->FindModule(mModuleSaveData.GetString("target")));
}
