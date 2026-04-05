/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/

#include "TopologySynth.h"
#include "OpenFrameworksPort.h"
#include "SynthGlobals.h"
#include "IAudioReceiver.h"
#include "ModularSynth.h"
#include "Profiler.h"
#include "ofxJSONElement.h"
#include "UIControlMacros.h"
#include "nanovg/nanovg.h"
#include <cmath>

TopologySynth::TopologySynth()
: IAudioProcessor(gBufferSize)
, mWriteBuffer(gBufferSize)
, mScopeBuffer(1024)
{
   mAmpEnv.Set(5, 10, 0.8f, 400);
   UpdateDelayLengths();
}

TopologySynth::~TopologySynth()
{
}

void TopologySynth::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   // Stage 1: Exciter (top-left panel)
   mExciterDropdown = new DropdownList(this, "exciter", 8, 18, (int*)&mExciterType, 60);
   mExciteDecaySlider = new FloatSlider(this, "decay", 8, 36, 60, 13, &mExciteDecay, 0.9f, 0.999f, 3);

   mExciterDropdown->AddLabel("noise", kExciter_Noise);
   mExciterDropdown->AddLabel("impulse", kExciter_Impulse);
   mExciterDropdown->AddLabel("saw", kExciter_Saw);

   // Stage 2: Lattice (top-center panel)
   mNumNodesSlider = new IntSlider(this, "nodes", 83, 18, 55, 13, &mNumNodes, 3, kTopoMaxNodes);
   mBoundaryDropdown = new DropdownList(this, "topo", 83, 36, (int*)&mBoundary, 55);
   mReflectionSlider = new FloatSlider(this, "refl", 143, 18, 55, 13, &mReflection, 0, 0.9f);
   mDampingSlider = new FloatSlider(this, "sustain", 143, 36, 55, 13, &mDamping, 0.999f, 0.99999f, 5);
   mExciteNodeSlider = new IntSlider(this, "hit", 203, 18, 40, 13, &mExciteNode, 0, mNumNodes - 1);

   mBoundaryDropdown->AddLabel("fixed", kTopo_Fixed);
   mBoundaryDropdown->AddLabel("ring", kTopo_Ring);
   mBoundaryDropdown->AddLabel("mobius", kTopo_Mobius);

   // Stage 3: Shaper (top-right panel)
   mShaperDropdown = new DropdownList(this, "shape", 253, 18, (int*)&mShaperType, 55);
   mShaperDriveSlider = new FloatSlider(this, "drive", 253, 36, 55, 13, &mShaperDrive, 0.5f, 6.0f);
   mShaperMixSlider = new FloatSlider(this, "mix", 313, 18, 50, 13, &mShaperMix, 0, 1);

   mShaperDropdown->AddLabel("off", kShaper_None);
   mShaperDropdown->AddLabel("tanh", kShaper_Tanh);
   mShaperDropdown->AddLabel("fold", kShaper_Fold);
   mShaperDropdown->AddLabel("rect", kShaper_Rectify);

   // Stage 4+5: Modal + Amp (below the panels)
   mModalBrightnessSlider = new FloatSlider(this, "bright", 8, 193, 80, 13, &mModalBrightness, 0, 1);
   mModalQSlider = new FloatSlider(this, "Q", 93, 193, 60, 13, &mModalQ, 1, 50);
   mVolumeSlider = new FloatSlider(this, "vol", 158, 193, 60, 13, &mVolume, 0, 1);
   mAmpEnvDisplay = new ADSRDisplay(this, "env", 223, 186, 140, 24, &mAmpEnv);
}

void TopologySynth::IntSliderUpdated(IntSlider* slider, int oldVal, double time)
{
   if (slider == mNumNodesSlider)
   {
      mExciteNodeSlider->SetExtents(0, mNumNodes - 1);
      if (mExciteNode >= mNumNodes) mExciteNode = 1;
      UpdateDelayLengths();
   }
}

void TopologySynth::DropdownUpdated(DropdownList* list, int oldVal, double time)
{
   if (list == mBoundaryDropdown)
      UpdateDelayLengths();
}

// ============================================================
// DSP
// ============================================================

void TopologySynth::UpdateDelayLengths()
{
   float totalDelay = gSampleRate / std::max(mFrequency, 20.0f);
   bool isRing = (mBoundary == kTopo_Ring || mBoundary == kTopo_Mobius);
   int edges = isRing ? mNumNodes : std::max(1, mNumNodes - 1);
   float perEdge = isRing ? totalDelay / edges : totalDelay / (2.0f * edges);
   int intDelay = std::max(2, std::min((int)perEdge, kTopoDelayMax - 1));
   for (int i = 0; i < mNumNodes; ++i)
      mNodes[i].delayLen = intDelay;
}

float TopologySynth::AllpassRead(float* buf, int wp, int len, float delay, float& apState)
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

float TopologySynth::ApplyShaper(float x, TopoShaperType type, float drive)
{
   switch (type)
   {
      case kShaper_None: return x;
      case kShaper_Tanh: return tanhf(x * drive);
      case kShaper_Fold: { float v = x * drive * 0.25f + 0.25f; v -= floorf(v); return fabsf(v * 4.0f - 2.0f) - 1.0f; }
      case kShaper_Rectify: return fabsf(x * drive) * (2.0f / std::max(drive, 0.5f));
      default: return x;
   }
}

void TopologySynth::ClearState()
{
   for (int i = 0; i < kTopoMaxNodes; ++i)
   {
      memset(mNodes[i].delayFwd, 0, sizeof(mNodes[i].delayFwd));
      memset(mNodes[i].delayBwd, 0, sizeof(mNodes[i].delayBwd));
      mNodes[i].fwd = mNodes[i].bwd = 0;
      mNodes[i].dcState = 0;
      mNodes[i].writePos = 0;
      mNodes[i].apStateFwd = mNodes[i].apStateBwd = 0;
   }
   for (int i = 0; i < 3; ++i)
      mModalZ1[i] = mModalZ2[i] = 0;
}

void TopologySynth::PlayNote(NoteMessage note)
{
   if (note.velocity > 0)
   {
      mPitch = note.pitch;
      mFrequency = 440.0f * powf(2.0f, (mPitch - 69.0f) / 12.0f);
      UpdateDelayLengths();
      ClearState();
      mAmpEnv.Start(gTime, note.velocity / 127.0f);
      mExciteAmount = note.velocity / 127.0f;
      mNoteOn = true;

      // Set modal frequencies based on topology
      bool isRing = (mBoundary == kTopo_Ring || mBoundary == kTopo_Mobius);
      if (isRing)
      {
         mModalFreqs[0] = 1.0f;
         mModalFreqs[1] = 2.0f;
         mModalFreqs[2] = 3.0f; // harmonic for ring
      }
      else
      {
         mModalFreqs[0] = 1.0f;
         mModalFreqs[1] = 2.76f;
         mModalFreqs[2] = 5.40f; // inharmonic for fixed (drum-like)
      }
      if (mBoundary == kTopo_Mobius)
      {
         mModalFreqs[0] = 0.5f; // octave down from Mobius twist
         mModalFreqs[1] = 1.5f;
         mModalFreqs[2] = 2.5f;
      }
   }
   else
   {
      mAmpEnv.Stop(gTime);
      mNoteOn = false;
   }
}

void TopologySynth::Process(double time)
{
   PROFILER(TopologySynth);

   IAudioReceiver* target = GetTarget();
   if (!mEnabled || target == nullptr)
      return;

   SyncBuffers(1);
   ComputeSliders(0);

   int bufferSize = target->GetBuffer()->BufferSize();
   mWriteBuffer.Clear();
   float* out = mWriteBuffer.GetChannel(0);

   bool isRing = (mBoundary == kTopo_Ring || mBoundary == kTopo_Mobius);
   int edges = isRing ? mNumNodes : mNumNodes - 1;

   // Precompute modal biquad coefficients (once per buffer)
   float mBQ_b0[3], mBQ_b1[3], mBQ_b2[3], mBQ_a1[3], mBQ_a2[3];
   for (int m = 0; m < 3; ++m)
   {
      float f = mFrequency * mModalFreqs[m];
      if (f > gSampleRate * 0.45f) f = gSampleRate * 0.45f;
      float w0 = FTWO_PI * f / gSampleRate;
      float alpha = sinf(w0) / (2.0f * mModalQ);
      float a0 = 1.0f + alpha;
      mBQ_b0[m] = alpha / a0;
      mBQ_b1[m] = 0;
      mBQ_b2[m] = -alpha / a0;
      mBQ_a1[m] = (-2.0f * cosf(w0)) / a0;
      mBQ_a2[m] = (1.0f - alpha) / a0;
   }

   float dcCoeff = expf(-FTWO_PI * 20.0f / gSampleRate); // correct DC blocker

   for (int s = 0; s < bufferSize; ++s)
   {
      mEnvValue = mAmpEnv.Value(time);

      // --- Stage 1: Exciter ---
      float excite = 0;
      if (mExciteAmount > 0.001f)
      {
         switch (mExciterType)
         {
            case kExciter_Noise: excite = mExciteAmount * RandomSample(); break;
            case kExciter_Impulse: excite = mExciteAmount; mExciteAmount = 0; break;
            case kExciter_Saw:
            {
               // Phase is per-instance (member mExciteAmount doubles as phase when negative won't happen)
               float phase = fmodf((float)(gTime * mFrequency / gSampleRate * 0.001f), 1.0f);
               excite = mExciteAmount * (phase * 2.0f - 1.0f);
               break;
            }
         }
         if (mExciterType != kExciter_Impulse)
            mExciteAmount *= mExciteDecay;
      }

      // Inject into lattice
      if (mExciteNode >= 0 && mExciteNode < mNumNodes)
      {
         mNodes[mExciteNode].fwd += excite;
         mNodes[mExciteNode].bwd -= excite * 0.3f;
      }

      // --- Stage 2: Lattice waveguide ---
      // Scatter (Kelly-Lochbaum unitary)
      float theta = mReflection * (FPI * 0.5f);
      float sc = cosf(theta), ss = sinf(theta);
      for (int i = 0; i < mNumNodes; ++i)
      {
         float f = mNodes[i].fwd, b = mNodes[i].bwd;
         float of = sc * f + ss * b;
         float ob = ss * f - sc * b;
         // DC blocker
         float dcPrev = mNodes[i].dcState;
         mNodes[i].dcState = of;
         of = of - dcPrev + dcCoeff * (of - dcPrev + dcPrev); // simplified
         // Actually: proper DC blocker: y = x - x_prev + alpha * y_prev
         // But we need y_prev state too. Simplify: just highpass the output later.
         mNodes[i].fwd = of * mDamping;
         mNodes[i].bwd = ob * mDamping;
      }

      // Snapshot + propagate
      float fSnap[kTopoMaxNodes], bSnap[kTopoMaxNodes];
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
         mNodes[next].fwd = AllpassRead(src.delayFwd, src.writePos, src.delayLen, src.delayLen - 1, src.apStateFwd);
      }
      for (int i = edges - 1; i >= 0; --i)
      {
         int next = (i + 1) % mNumNodes;
         auto& src = mNodes[next];
         src.delayBwd[src.writePos] = bSnap[next];
         mNodes[i].bwd = AllpassRead(src.delayBwd, src.writePos, src.delayLen, src.delayLen - 1, src.apStateBwd);
      }

      // Boundary conditions
      if (mBoundary == kTopo_Mobius)
      {
         mNodes[0].fwd = -mNodes[0].fwd;
         mNodes[mNumNodes - 1].bwd = -mNodes[mNumNodes - 1].bwd;
      }
      else if (mBoundary == kTopo_Fixed && !isRing)
      {
         mNodes[0].bwd = -mNodes[0].fwd;
         mNodes[mNumNodes - 1].fwd = -mNodes[mNumNodes - 1].bwd;
      }

      for (int i = 0; i < mNumNodes; ++i)
         mNodes[i].writePos = (mNodes[i].writePos + 1) % std::max(2, mNodes[i].delayLen);

      // Pickup: sum all nodes
      float latticeOut = 0;
      for (int i = 0; i < mNumNodes; ++i)
         latticeOut += mNodes[i].fwd + mNodes[i].bwd;
      latticeOut /= mNumNodes;

      // --- Stage 3: Shaper ---
      float dry = latticeOut;
      float shaped = ApplyShaper(latticeOut, mShaperType, mShaperDrive);
      float shaperOut = dry * (1.0f - mShaperMix) + shaped * mShaperMix;

      // --- Stage 4: Modal filter (3 resonant bandpasses) ---
      float modalOut = 0;
      float modalIn = shaperOut;
      for (int m = 0; m < 3; ++m)
      {
         float amp = mModalAmps[m] * (m == 0 ? 1.0f : mModalBrightness);
         float y = mBQ_b0[m] * modalIn + mBQ_b1[m] * 0 + mBQ_b2[m] * (-mModalZ2[m])
                   - mBQ_a1[m] * mModalZ1[m] - mBQ_a2[m] * mModalZ2[m];
         mModalZ2[m] = mModalZ1[m];
         mModalZ1[m] = y;
         modalOut += y * amp;
      }

      // Blend lattice direct + modal
      float finalOut = latticeOut * 0.6f + modalOut * 0.4f;

      // --- Stage 5: Amp ---
      out[s] = ofClamp(finalOut * mVolume * mEnvValue, -2.0f, 2.0f);

      // Viz
      mScopeBuffer.Write(out[s], 0);

      time += gInvSampleRateMs;
   }

   // Store viz state (once per buffer)
   for (int i = 0; i < mNumNodes; ++i)
      mNodeViz[i] = mNodes[i].fwd + mNodes[i].bwd;

   // Output
   GetVizBuffer()->WriteChunk(out, bufferSize, 0);
   Add(target->GetBuffer()->GetChannel(0), out, bufferSize);
   GetBuffer()->Reset();
}

// ============================================================
// SKEUOMORPHIC VISUALIZATION
// ============================================================

void TopologySynth::GetModuleDimensions(float& width, float& height)
{
   width = 370;
   height = 218;
}

void TopologySynth::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   ofColor color = GetColor(kModuleCategory_Synth);
   float panelH = 40;
   float panelY = 5;

   // ---- Panel backgrounds (3 stages across the top) ----

   // Panel 1: Exciter
   {
      NVGpaint p = nvgLinearGradient(gNanoVG, 3, panelY, 3, panelY + panelH + 8,
         nvgRGBA(35, 25, 20, (int)(gModuleDrawAlpha * .7f)),
         nvgRGBA(20, 15, 12, (int)(gModuleDrawAlpha * .7f)));
      nvgBeginPath(gNanoVG);
      nvgRoundedRect(gNanoVG, 3, panelY, 72, panelH + 8, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, p);
      nvgFill(gNanoVG);
      ofSetColor(180, 130, 80, gModuleDrawAlpha * .4f);
      DrawTextNormal("EXCITE", 8, panelY + 11, 8);
   }

   // Panel 2: Lattice
   {
      NVGpaint p = nvgLinearGradient(gNanoVG, 78, panelY, 78, panelY + panelH + 8,
         nvgRGBA(20, 30, 35, (int)(gModuleDrawAlpha * .7f)),
         nvgRGBA(12, 18, 22, (int)(gModuleDrawAlpha * .7f)));
      nvgBeginPath(gNanoVG);
      nvgRoundedRect(gNanoVG, 78, panelY, 168, panelH + 8, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, p);
      nvgFill(gNanoVG);
      ofSetColor(80, 160, 180, gModuleDrawAlpha * .4f);
      DrawTextNormal("LATTICE", 83, panelY + 11, 8);
   }

   // Panel 3: Shaper
   {
      NVGpaint p = nvgLinearGradient(gNanoVG, 249, panelY, 249, panelY + panelH + 8,
         nvgRGBA(35, 20, 30, (int)(gModuleDrawAlpha * .7f)),
         nvgRGBA(22, 12, 18, (int)(gModuleDrawAlpha * .7f)));
      nvgBeginPath(gNanoVG);
      nvgRoundedRect(gNanoVG, 249, panelY, 118, panelH + 8, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, p);
      nvgFill(gNanoVG);
      ofSetColor(180, 80, 130, gModuleDrawAlpha * .4f);
      DrawTextNormal("SHAPER", 253, panelY + 11, 8);
   }

   // Draw controls
   mExciterDropdown->Draw();
   mExciteDecaySlider->Draw();
   mNumNodesSlider->Draw();
   mBoundaryDropdown->Draw();
   mReflectionSlider->Draw();
   mDampingSlider->Draw();
   mExciteNodeSlider->Draw();
   mShaperDropdown->Draw();
   mShaperDriveSlider->Draw();
   mShaperMixSlider->Draw();
   mModalBrightnessSlider->Draw();
   mModalQSlider->Draw();
   mVolumeSlider->Draw();
   mAmpEnvDisplay->Draw();

   // ---- Signal flow arrows between panels ----
   ofSetColor(color.r, color.g, color.b, gModuleDrawAlpha * .25f);
   ofSetLineWidth(1);
   float arrowY = panelY + panelH / 2 + 4;
   // Exciter → Lattice
   ofLine(75, arrowY, 78, arrowY);
   ofCircle(78, arrowY, 2);
   // Lattice → Shaper
   ofLine(246, arrowY, 249, arrowY);
   ofCircle(249, arrowY, 2);

   // ---- Lattice visualization (center area) ----
   {
      float vizX = 5, vizY = 58, vizW = 360, vizH = 125;

      // Dark background
      NVGpaint bg = nvgRadialGradient(gNanoVG, vizX + vizW / 2, vizY + vizH / 2, 10, vizW * 0.5f,
         nvgRGBA(10, 12, 18, (int)(gModuleDrawAlpha * .8f)),
         nvgRGBA(4, 5, 10, (int)(gModuleDrawAlpha * .85f)));
      nvgBeginPath(gNanoVG);
      nvgRoundedRect(gNanoVG, vizX, vizY, vizW, vizH, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, bg);
      nvgFill(gNanoVG);

      bool isRing = (mBoundary == kTopo_Ring || mBoundary == kTopo_Mobius);
      float cx = vizX + vizW / 2;
      float cy = vizY + vizH / 2;

      // Node positions
      float nx[kTopoMaxNodes], ny[kTopoMaxNodes];
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
            nx[i] = vizX + 20 + t * (vizW - 40);
            ny[i] = cy;
         }
      }

      // Edges
      int edgeCount = isRing ? mNumNodes : mNumNodes - 1;
      for (int i = 0; i < edgeCount; ++i)
      {
         int j = (i + 1) % mNumNodes;
         float en = (fabsf(mNodeViz[i]) + fabsf(mNodeViz[j])) * 3;
         float m = ofClamp(en, 0, 1);
         ofSetColor(color.r * (.2f + m * .6f), color.g * (.2f + m * .6f), color.b * (.2f + m * .6f),
                    gModuleDrawAlpha * (.2f + m * .6f));
         ofSetLineWidth(.8f + m * 2.0f);
         ofLine(nx[i], ny[i], nx[j], ny[j]);
      }

      // Mobius marker
      if (mBoundary == kTopo_Mobius && mNumNodes > 0)
      {
         float mx = (nx[mNumNodes - 1] + nx[0]) / 2;
         float my = (ny[mNumNodes - 1] + ny[0]) / 2;
         ofSetColor(255, 100, 100, gModuleDrawAlpha * .5f);
         ofSetLineWidth(1.5f);
         ofLine(mx - 4, my - 4, mx + 4, my + 4);
         ofLine(mx - 4, my + 4, mx + 4, my - 4);
      }

      // Nodes
      for (int i = 0; i < mNumNodes; ++i)
      {
         float amp = mNodeViz[i];
         float m = ofClamp(fabsf(amp) * 5, 0, 1);
         float disp = isRing ? 0 : amp * 30;
         float x = nx[i], y = ny[i] + disp;

         if (m > 0.01f)
         {
            NVGpaint glow = nvgRadialGradient(gNanoVG, x, y, 2, 14,
               nvgRGBA(color.r, color.g, color.b, (int)(m * gModuleDrawAlpha * .4f)),
               nvgRGBA(color.r, color.g, color.b, 0));
            nvgBeginPath(gNanoVG);
            nvgCircle(gNanoVG, x, y, 16);
            nvgFillPaint(gNanoVG, glow);
            nvgFill(gNanoVG);
         }

         float r = 3 + m * 3;
         ofFill();
         ofSetColor(color.r * (.4f + m * .6f), color.g * (.4f + m * .6f), color.b * (.4f + m * .6f),
                    gModuleDrawAlpha * (.6f + m * .4f));
         ofCircle(x, y, r);

         if (i == mExciteNode)
         {
            ofNoFill();
            ofSetColor(255, 255, 255, gModuleDrawAlpha * .4f);
            ofSetLineWidth(1);
            ofCircle(x, y, r + 3);
         }
      }

      // For chain: string shape
      if (!isRing && mNumNodes > 1)
      {
         ofSetColor(color.r, color.g, color.b, gModuleDrawAlpha * .3f);
         ofSetLineWidth(1);
         ofNoFill();
         ofBeginShape();
         for (int i = 0; i < mNumNodes; ++i)
            ofVertex(nx[i], ny[i] + mNodeViz[i] * 30);
         ofEndShape(false);
      }

      // Output scope (bottom-right of viz area)
      {
         float scopeX = vizX + vizW - 100;
         float scopeY = vizY + vizH - 30;
         float scopeW = 90;
         float scopeH = 24;

         ofSetColor(color.r * .3f, color.g * .3f, color.b * .3f, gModuleDrawAlpha * .4f);
         ofFill();
         ofRect(scopeX, scopeY, scopeW, scopeH, gCornerRoundness);

         ofSetColor(color.r, color.g, color.b, gModuleDrawAlpha * .6f);
         ofSetLineWidth(1);
         ofNoFill();
         ofBeginShape();
         for (int i = 0; i < (int)scopeW; ++i)
         {
            int idx = mScopeBuffer.Size() - (int)scopeW + i;
            if (idx < 0) idx = 0;
            float sample = mScopeBuffer.GetSample(mScopeBuffer.Size() - 1 - i, 0);
            float y = scopeY + scopeH / 2 - sample * scopeH * 2;
            ofVertex(scopeX + i, ofClamp(y, scopeY, scopeY + scopeH));
         }
         ofEndShape(false);
      }

      // Topology label
      const char* topoStr = mBoundary == kTopo_Fixed ? "chain" :
                             mBoundary == kTopo_Ring ? "ring" : "mobius";
      ofSetColor(255, 255, 255, gModuleDrawAlpha * .15f);
      DrawTextNormal(topoStr, vizX + 5, vizY + vizH - 5, 8);
   }
}

// ============================================================
// SAVE/LOAD
// ============================================================

void TopologySynth::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadInt("nodes", moduleInfo, 6, 3, kTopoMaxNodes, true);
   SetUpFromSaveData();
}

void TopologySynth::SetUpFromSaveData()
{
   mNumNodes = mModuleSaveData.GetInt("nodes");
   UpdateDelayLengths();
}
