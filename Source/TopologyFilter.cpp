/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/

#include "TopologyFilter.h"
#include "OpenFrameworksPort.h"
#include "SynthGlobals.h"
#include "IAudioReceiver.h"
#include "ModularSynth.h"
#include "Profiler.h"
#include "ofxJSONElement.h"
#include "UIControlMacros.h"
#include "nanovg/nanovg.h"
#include <cmath>

TopologyFilter::TopologyFilter()
: IAudioProcessor(gBufferSize)
{
   UpdateDelayLengths();
}

TopologyFilter::~TopologyFilter()
{
}

void TopologyFilter::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   UIBLOCK0();
   INTSLIDER(mNumNodesSlider, "nodes", &mNumNodes, 3, kFiltMaxNodes);
   DROPDOWN(mBoundaryDropdown, "topo", (int*)&mBoundary, 70);
   FLOATSLIDER(mReflectionSlider, "reflect", &mReflection, 0, 0.9f);
   FLOATSLIDER_DIGITS(mDampingSlider, "damp", &mDamping, 0.98f, 0.9999f, 4);
   FLOATSLIDER(mDriveSlider, "drive", &mDrive, 0.5f, 6.0f);
   DROPDOWN(mCorruptionDropdown, "corrupt", &mCorruptionType, 70);
   INTSLIDER(mInjectSlider, "inject", &mInjectNode, 0, mNumNodes - 1);
   INTSLIDER(mPickupSlider, "pickup", &mPickupNode, 0, mNumNodes - 1);
   FLOATSLIDER(mWetDrySlider, "mix", &mWetDry, 0, 1);
   ENDUIBLOCK0();

   mBoundaryDropdown->AddLabel("fixed", kFilt_Fixed);
   mBoundaryDropdown->AddLabel("ring", kFilt_Ring);
   mBoundaryDropdown->AddLabel("mobius", kFilt_Mobius);

   mCorruptionDropdown->AddLabel("none", kFiltCorr_None);
   mCorruptionDropdown->AddLabel("tanh", kFiltCorr_Tanh);
   mCorruptionDropdown->AddLabel("fold", kFiltCorr_Fold);
}

void TopologyFilter::IntSliderUpdated(IntSlider* slider, int oldVal, double time)
{
   if (slider == mNumNodesSlider)
   {
      mInjectSlider->SetExtents(0, mNumNodes - 1);
      mPickupSlider->SetExtents(0, mNumNodes - 1);
      if (mInjectNode >= mNumNodes) mInjectNode = 0;
      if (mPickupNode >= mNumNodes) mPickupNode = mNumNodes - 1;
      UpdateDelayLengths();
   }
}

void TopologyFilter::DropdownUpdated(DropdownList* list, int oldVal, double time)
{
   if (list == mBoundaryDropdown)
      UpdateDelayLengths();
}

void TopologyFilter::UpdateDelayLengths()
{
   float totalDelay = gSampleRate / std::max(mFrequency, 20.0f);
   bool isRing = (mBoundary == kFilt_Ring || mBoundary == kFilt_Mobius);
   int edges = isRing ? mNumNodes : std::max(1, mNumNodes - 1);
   float perEdge = isRing ? totalDelay / edges : totalDelay / (2.0f * edges);
   int intDelay = std::max(2, std::min((int)perEdge, kFiltDelayMax - 1));
   for (int i = 0; i < mNumNodes; ++i)
      mNodes[i].delayLen = intDelay;
}

float TopologyFilter::AllpassRead(float* buf, int wp, int len, float delay, float& apState)
{
   if (len <= 1) return buf[0];
   int intD = (int)delay;
   float frac = delay - intD;
   int idx = (wp - intD + len * 2) % len;
   if (frac < 0.001f) { apState = buf[idx]; return buf[idx]; }
   float a = (1.0f - frac) / (1.0f + frac);
   int idxP = (idx - 1 + len) % len;
   float y = a * buf[idx] + buf[idxP] - a * apState;
   apState = y;
   return y;
}

void TopologyFilter::PlayNote(NoteMessage note)
{
   if (note.velocity > 0)
   {
      mFrequency = 440.0f * powf(2.0f, (note.pitch - 69.0f) / 12.0f);
      UpdateDelayLengths();
   }
}

void TopologyFilter::Process(double time)
{
   PROFILER(TopologyFilter);

   IAudioReceiver* target = GetTarget();
   if (!mEnabled || target == nullptr)
      return;

   SyncBuffers();
   ComputeSliders(0);

   int bufferSize = GetBuffer()->BufferSize();
   float* input = GetBuffer()->GetChannel(0);

   bool isRing = (mBoundary == kFilt_Ring || mBoundary == kFilt_Mobius);
   int edges = isRing ? mNumNodes : mNumNodes - 1;

   float theta = mReflection * (FPI * 0.5f);
   float sc = cosf(theta), ss = sinf(theta);

   for (int s = 0; s < bufferSize; ++s)
   {
      float dry = input[s];

      // Inject input audio into the lattice at the injection node
      if (mInjectNode >= 0 && mInjectNode < mNumNodes)
         mNodes[mInjectNode].fwd += dry * 0.5f;

      // Scatter (unitary Kelly-Lochbaum)
      for (int i = 0; i < mNumNodes; ++i)
      {
         float f = mNodes[i].fwd, b = mNodes[i].bwd;
         float of = sc * f + ss * b;
         float ob = ss * f - sc * b;

         // Corruption
         if (mCorruptionType == kFiltCorr_Tanh)
         {
            of = tanhf(of * mDrive);
            ob = tanhf(ob * mDrive);
         }
         else if (mCorruptionType == kFiltCorr_Fold)
         {
            float vf = of * mDrive * 0.25f + 0.25f; vf -= floorf(vf);
            of = fabsf(vf * 4.0f - 2.0f) - 1.0f;
            float vb = ob * mDrive * 0.25f + 0.25f; vb -= floorf(vb);
            ob = fabsf(vb * 4.0f - 2.0f) - 1.0f;
         }

         mNodes[i].fwd = of * mDamping;
         mNodes[i].bwd = ob * mDamping;
      }

      // Snapshot + propagate
      float fSnap[kFiltMaxNodes], bSnap[kFiltMaxNodes];
      for (int i = 0; i < mNumNodes; ++i)
      {
         fSnap[i] = mNodes[i].fwd;
         bSnap[i] = mNodes[i].bwd;
      }

      for (int i = 0; i < edges; ++i)
      {
         int next = (i + 1) % mNumNodes;
         auto& src = mNodes[i];
         src.delayFwd[src.writePos] = fSnap[i];
         mNodes[next].fwd = AllpassRead(src.delayFwd, src.writePos, src.delayLen, src.delayLen - 1, src.apFwd);
      }
      for (int i = edges - 1; i >= 0; --i)
      {
         int next = (i + 1) % mNumNodes;
         auto& src = mNodes[next];
         src.delayBwd[src.writePos] = bSnap[next];
         mNodes[i].bwd = AllpassRead(src.delayBwd, src.writePos, src.delayLen, src.delayLen - 1, src.apBwd);
      }

      // Boundary
      if (mBoundary == kFilt_Mobius)
      {
         mNodes[0].fwd = -mNodes[0].fwd;
         mNodes[mNumNodes - 1].bwd = -mNodes[mNumNodes - 1].bwd;
      }
      else if (mBoundary == kFilt_Fixed && !isRing)
      {
         mNodes[0].bwd = -mNodes[0].fwd;
         mNodes[mNumNodes - 1].fwd = -mNodes[mNumNodes - 1].bwd;
      }

      for (int i = 0; i < mNumNodes; ++i)
         mNodes[i].writePos = (mNodes[i].writePos + 1) % std::max(2, mNodes[i].delayLen);

      // Pickup
      float wet = 0;
      if (mPickupNode >= 0 && mPickupNode < mNumNodes)
         wet = mNodes[mPickupNode].fwd + mNodes[mPickupNode].bwd;

      // Mix
      input[s] = ofClamp(dry * (1.0f - mWetDry) + wet * mWetDry, -4.0f, 4.0f);
   }

   // Viz (once per buffer)
   for (int i = 0; i < mNumNodes; ++i)
      mNodeViz[i] = mNodes[i].fwd + mNodes[i].bwd;

   // Pass through to target
   for (int ch = 0; ch < GetBuffer()->NumActiveChannels(); ++ch)
   {
      Add(target->GetBuffer()->GetChannel(ch), GetBuffer()->GetChannel(ch), bufferSize);
      GetVizBuffer()->WriteChunk(GetBuffer()->GetChannel(ch), bufferSize, ch);
   }
   GetBuffer()->Reset();
}

// ============================================================
// VISUALIZATION
// ============================================================

void TopologyFilter::GetModuleDimensions(float& width, float& height)
{
   width = 220;
   height = 280;
}

void TopologyFilter::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   // Controls
   mNumNodesSlider->Draw();
   mBoundaryDropdown->Draw();
   mReflectionSlider->Draw();
   mDampingSlider->Draw();
   mDriveSlider->Draw();
   mCorruptionDropdown->Draw();
   mInjectSlider->Draw();
   mPickupSlider->Draw();
   mWetDrySlider->Draw();

   // Lattice viz
   float vizX = 5, vizY = 160, vizW = 210, vizH = 110;
   float cx = vizX + vizW / 2, cy = vizY + vizH / 2;

   {
      NVGpaint bg = nvgRadialGradient(gNanoVG, cx, cy, 5, vizW * 0.5f,
         nvgRGBA(10, 14, 20, (int)(gModuleDrawAlpha * .8f)),
         nvgRGBA(4, 6, 10, (int)(gModuleDrawAlpha * .85f)));
      nvgBeginPath(gNanoVG);
      nvgRoundedRect(gNanoVG, vizX, vizY, vizW, vizH, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, bg);
      nvgFill(gNanoVG);
   }

   ofColor color = GetColor(kModuleCategory_Audio); // audio category = cyan

   bool isRing = (mBoundary == kFilt_Ring || mBoundary == kFilt_Mobius);
   float nx[kFiltMaxNodes], ny[kFiltMaxNodes];

   if (isRing)
   {
      float rad = std::min(vizW, vizH) * 0.35f;
      for (int i = 0; i < mNumNodes; ++i)
      {
         float a = (float)i / mNumNodes * FTWO_PI - FPI / 2;
         nx[i] = cx + cosf(a) * rad;
         ny[i] = cy + sinf(a) * rad;
      }
   }
   else
   {
      for (int i = 0; i < mNumNodes; ++i)
      {
         float t = mNumNodes > 1 ? (float)i / (mNumNodes - 1) : 0.5f;
         nx[i] = vizX + 15 + t * (vizW - 30);
         ny[i] = cy;
      }
   }

   // Edges
   int edgeCount = isRing ? mNumNodes : mNumNodes - 1;
   for (int i = 0; i < edgeCount; ++i)
   {
      int j = (i + 1) % mNumNodes;
      float m = ofClamp((fabsf(mNodeViz[i]) + fabsf(mNodeViz[j])) * 3, 0, 1);
      ofSetColor(color.r * (.2f + m * .6f), color.g * (.2f + m * .6f), color.b * (.2f + m * .6f),
                 gModuleDrawAlpha * (.2f + m * .6f));
      ofSetLineWidth(.8f + m * 2);
      ofLine(nx[i], ny[i], nx[j], ny[j]);
   }

   // Nodes
   for (int i = 0; i < mNumNodes; ++i)
   {
      float m = ofClamp(fabsf(mNodeViz[i]) * 5, 0, 1);
      float disp = isRing ? 0 : mNodeViz[i] * 25;

      if (m > 0.01f)
      {
         NVGpaint glow = nvgRadialGradient(gNanoVG, nx[i], ny[i] + disp, 2, 12,
            nvgRGBA(color.r, color.g, color.b, (int)(m * gModuleDrawAlpha * .4f)),
            nvgRGBA(color.r, color.g, color.b, 0));
         nvgBeginPath(gNanoVG);
         nvgCircle(gNanoVG, nx[i], ny[i] + disp, 14);
         nvgFillPaint(gNanoVG, glow);
         nvgFill(gNanoVG);
      }

      ofFill();
      ofSetColor(color.r * (.4f + m * .6f), color.g * (.4f + m * .6f), color.b * (.4f + m * .6f),
                 gModuleDrawAlpha * (.6f + m * .4f));
      ofCircle(nx[i], ny[i] + disp, 3 + m * 3);

      // Inject marker (arrow in)
      if (i == mInjectNode)
      {
         ofSetColor(255, 200, 100, gModuleDrawAlpha * .5f);
         DrawTextNormal("IN", nx[i] - 5, ny[i] + disp - 10, 7);
      }
      // Pickup marker (arrow out)
      if (i == mPickupNode)
      {
         ofSetColor(100, 200, 255, gModuleDrawAlpha * .5f);
         DrawTextNormal("OUT", nx[i] - 7, ny[i] + disp + 14, 7);
      }
   }

   // Topology label
   const char* topoStr = mBoundary == kFilt_Fixed ? "fixed" :
                          mBoundary == kFilt_Ring ? "ring" : "mobius";
   ofSetColor(255, 255, 255, gModuleDrawAlpha * .15f);
   DrawTextNormal(topoStr, vizX + 5, vizY + vizH - 4, 8);
}

void TopologyFilter::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadInt("nodes", moduleInfo, 5, 3, kFiltMaxNodes, true);
   SetUpFromSaveData();
}

void TopologyFilter::SetUpFromSaveData()
{
   mNumNodes = mModuleSaveData.GetInt("nodes");
   UpdateDelayLengths();
}
