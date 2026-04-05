/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/

#include "FMCluster.h"
#include "OpenFrameworksPort.h"
#include "SynthGlobals.h"
#include "IAudioReceiver.h"
#include "ModularSynth.h"
#include "Profiler.h"
#include "ofxJSONElement.h"
#include "UIControlMacros.h"
#include "nanovg/nanovg.h"
#include <cmath>

// Default modulation routing: op0→op1, op2→op3, simple 2-pair
static const float kDefaultMod[kFMMaxOps][kFMMaxOps] = {
   { 0, 0, 0, 0, 0, 0 },
   { 2, 0, 0, 0, 0, 0 },  // op0 modulates op1
   { 0, 0, 0, 0, 0, 0 },
   { 0, 0, 3, 0, 0, 0 },  // op2 modulates op3
   { 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0 },
};

// Default ratios: 1, 1, 2, 1, 3, 4
static const float kDefaultRatios[kFMMaxOps] = { 1, 1, 2, 1, 3, 4 };
// Default levels: carriers are ops 1 and 3
static const float kDefaultLevels[kFMMaxOps] = { 0, 1, 0, 0.7f, 0, 0 };

FMCluster::FMCluster()
: IAudioProcessor(gBufferSize)
, mWriteBuffer(gBufferSize)
{
   mEnvelope.Set(3, 10, 0.7f, 300);
   for (int i = 0; i < kFMMaxOps; ++i)
   {
      mOps[i].ratio = kDefaultRatios[i];
      mOps[i].outputLevel = kDefaultLevels[i];
      for (int j = 0; j < kFMMaxOps; ++j)
         mModDepth[i][j] = kDefaultMod[i][j];
   }
}

FMCluster::~FMCluster()
{
}

void FMCluster::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   UIBLOCK0();
   INTSLIDER(mNumOpsSlider, "ops", &mNumOps, 2, kFMMaxOps);
   FLOATSLIDER(mFeedbackSlider, "feedback", &mFeedback, 0, 5);
   FLOATSLIDER(mBrightnessSlider, "bright", &mBrightness, 0, 3);
   FLOATSLIDER(mVolumeSlider, "vol", &mVolume, 0, 1);
   ENDUIBLOCK0();

   // Per-operator controls
   for (int i = 0; i < kFMMaxOps; ++i)
   {
      char name[32];
      snprintf(name, sizeof(name), "r%d", i);
      mRatioSliders[i] = new FloatSlider(this, name, 5 + (i % 3) * 70, 72 + (i / 3) * 18, 65, 13, &mOps[i].ratio, 0.25f, 12.0f);

      snprintf(name, sizeof(name), "l%d", i);
      mLevelSliders[i] = new FloatSlider(this, name, 5 + (i % 3) * 70, 108 + (i / 3) * 18, 65, 13, &mOps[i].outputLevel, 0, 1);
   }

   mEnvDisplay = new ADSRDisplay(this, "env", 5, 145, 200, 20, &mEnvelope);
}

void FMCluster::IntSliderUpdated(IntSlider* slider, int oldVal, double time)
{
   if (slider == mNumOpsSlider)
   {
      for (int i = 0; i < kFMMaxOps; ++i)
      {
         mRatioSliders[i]->SetShowing(i < mNumOps);
         mLevelSliders[i]->SetShowing(i < mNumOps);
      }
   }
}

void FMCluster::PlayNote(NoteMessage note)
{
   if (note.velocity > 0)
   {
      mFrequency = 440.0f * powf(2.0f, (note.pitch - 69.0f) / 12.0f);
      mEnvelope.Start(gTime, note.velocity / 127.0f);
      // Reset phases for consistent attack
      for (int i = 0; i < mNumOps; ++i)
         mOps[i].phase = 0;
   }
   else
   {
      mEnvelope.Stop(gTime);
   }
}

void FMCluster::Process(double time)
{
   PROFILER(FMCluster);

   IAudioReceiver* target = GetTarget();
   if (!mEnabled || target == nullptr)
      return;

   SyncBuffers(1);
   ComputeSliders(0);

   int bufferSize = target->GetBuffer()->BufferSize();
   mWriteBuffer.Clear();
   float* out = mWriteBuffer.GetChannel(0);

   // Precompute phase increments
   float phaseInc[kFMMaxOps];
   for (int i = 0; i < mNumOps; ++i)
      phaseInc[i] = FTWO_PI * mFrequency * mOps[i].ratio / gSampleRate;

   for (int s = 0; s < bufferSize; ++s)
   {
      mEnvValue = mEnvelope.Value(time);

      // Compute all operator outputs simultaneously
      float opOut[kFMMaxOps];

      for (int i = 0; i < mNumOps; ++i)
      {
         // Sum modulation from all operators
         float modSum = 0;
         for (int j = 0; j < mNumOps; ++j)
         {
            if (i == j)
               modSum += mFeedback * mOps[j].prevOutput; // self-feedback
            else
               modSum += mModDepth[i][j] * mBrightness * mOps[j].prevOutput;
         }

         // FM synthesis: sin(phase + modulation)
         // Wrap the argument to prevent float precision loss at large modSum
         float fmPhase = fmodf(mOps[i].phase + modSum, FTWO_PI);
         opOut[i] = sinf(fmPhase);

         // Advance phase
         mOps[i].phase += phaseInc[i];
         if (mOps[i].phase > FTWO_PI) mOps[i].phase -= FTWO_PI;
      }

      // Store outputs for next sample's modulation (one-sample delay for stability)
      float sample = 0;
      for (int i = 0; i < mNumOps; ++i)
      {
         mOps[i].prevOutput = opOut[i];
         sample += opOut[i] * mOps[i].outputLevel;
      }

      // Normalize by number of carriers (ops with outputLevel > 0)
      float carrierSum = 0;
      for (int i = 0; i < mNumOps; ++i)
         carrierSum += mOps[i].outputLevel;
      if (carrierSum > 0.01f)
         sample /= carrierSum;

      out[s] = ofClamp(sample * mVolume * mEnvValue, -2.0f, 2.0f);
      time += gInvSampleRateMs;
   }

   // Store viz state
   for (int i = 0; i < mNumOps; ++i)
      mOpViz[i] = fabsf(mOps[i].prevOutput) * mOps[i].outputLevel;

   GetVizBuffer()->WriteChunk(out, bufferSize, 0);
   Add(target->GetBuffer()->GetChannel(0), out, bufferSize);
   GetBuffer()->Reset();
}

// ============================================================
// VISUALIZATION — FM modulation graph
// ============================================================

void FMCluster::GetModuleDimensions(float& width, float& height)
{
   width = 280;
   height = 360;
}

void FMCluster::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   // Controls
   mNumOpsSlider->Draw();
   mFeedbackSlider->Draw();
   mBrightnessSlider->Draw();
   mVolumeSlider->Draw();

   for (int i = 0; i < mNumOps; ++i)
   {
      mRatioSliders[i]->Draw();
      mLevelSliders[i]->Draw();
   }

   mEnvDisplay->Draw();

   // === FM Graph Visualization ===
   float vizX = 10, vizY = 172, vizW = 260, vizH = 180;
   float cx = vizX + vizW / 2, cy = vizY + vizH / 2;

   // Background
   {
      NVGpaint bg = nvgRadialGradient(gNanoVG, cx, cy, 10, vizW * 0.5f,
         nvgRGBA(12, 10, 18, (int)(gModuleDrawAlpha * .85f)),
         nvgRGBA(4, 3, 8, (int)(gModuleDrawAlpha * .9f)));
      nvgBeginPath(gNanoVG);
      nvgRoundedRect(gNanoVG, vizX, vizY, vizW, vizH, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, bg);
      nvgFill(gNanoVG);
   }

   // Node positions (circle layout)
   float nodeX[kFMMaxOps], nodeY[kFMMaxOps];
   float rad = std::min(vizW, vizH) * 0.32f;
   for (int i = 0; i < mNumOps; ++i)
   {
      float angle = (float)i / mNumOps * FTWO_PI - FPI / 2;
      nodeX[i] = cx + cosf(angle) * rad;
      nodeY[i] = cy + sinf(angle) * rad;
   }

   // Draw modulation edges
   for (int i = 0; i < mNumOps; ++i)
   {
      for (int j = 0; j < mNumOps; ++j)
      {
         float depth = (i == j) ? mFeedback : mModDepth[i][j] * mBrightness;
         if (fabsf(depth) < 0.01f) continue;

         float mag = ofClamp(fabsf(depth) / 5.0f, 0, 1);

         if (i == j)
         {
            // Self-feedback: draw small arc
            float nx = nodeX[i], ny = nodeY[i];
            float arcR = 12 + mag * 8;
            float dirX = (nx - cx), dirY = (ny - cy);
            float len = sqrtf(dirX * dirX + dirY * dirY);
            if (len > 0.01f) { dirX /= len; dirY /= len; }
            float arcCx = nx + dirX * arcR;
            float arcCy = ny + dirY * arcR;

            ofPushStyle();
            ofNoFill();
            ofSetColor(255, 100, 100, gModuleDrawAlpha * (.3f + mag * .5f));
            ofSetLineWidth(0.8f + mag * 2);
            // Simple arc approximation
            ofBeginShape();
            for (int a = 0; a < 12; ++a)
            {
               float t = (float)a / 11 * FTWO_PI * 0.7f + atan2f(dirY, dirX) - FPI * 0.35f;
               ofVertex(arcCx + cosf(t) * arcR * 0.5f, arcCy + sinf(t) * arcR * 0.5f);
            }
            ofEndShape(false);
            ofPopStyle();
         }
         else
         {
            // Modulation edge: from j to i (j modulates i)
            // Color: positive depth = warm, negative = cool
            int cr = depth > 0 ? 255 : 80;
            int cg = 150;
            int cb = depth > 0 ? 80 : 255;

            ofPushStyle();
            ofSetColor(cr, cg, cb, gModuleDrawAlpha * (.15f + mag * .5f));
            ofSetLineWidth(0.5f + mag * 2.5f);
            ofLine(nodeX[j], nodeY[j], nodeX[i], nodeY[i]);

            // Arrow head at destination
            float dx = nodeX[i] - nodeX[j], dy = nodeY[i] - nodeY[j];
            float dlen = sqrtf(dx * dx + dy * dy);
            if (dlen > 1)
            {
               dx /= dlen; dy /= dlen;
               float ax = nodeX[i] - dx * 12;
               float ay = nodeY[i] - dy * 12;
               float perpX = -dy * 4 * mag, perpY = dx * 4 * mag;
               ofFill();
               ofTriangle(nodeX[i], nodeY[i], ax + perpX, ay + perpY, ax - perpX, ay - perpY);
            }
            ofPopStyle();
         }
      }
   }

   // Draw operator nodes
   ofColor color = GetColor(kModuleCategory_Synth);
   for (int i = 0; i < mNumOps; ++i)
   {
      float nx = nodeX[i], ny = nodeY[i];
      float outputMag = ofClamp(mOpViz[i] * 3, 0, 1);
      bool isCarrier = mOps[i].outputLevel > 0.01f;

      // Node glow
      if (outputMag > 0.01f)
      {
         int r = isCarrier ? 255 : (int)(color.r);
         int g = isCarrier ? 220 : (int)(color.g);
         int b = isCarrier ? 150 : (int)(color.b);

         NVGpaint glow = nvgRadialGradient(gNanoVG, nx, ny, 3, 18,
            nvgRGBA(r, g, b, (int)(outputMag * gModuleDrawAlpha * .4f)),
            nvgRGBA(r, g, b, 0));
         nvgBeginPath(gNanoVG);
         nvgCircle(gNanoVG, nx, ny, 22);
         nvgFillPaint(gNanoVG, glow);
         nvgFill(gNanoVG);
      }

      // Node circle — carriers larger and brighter
      float radius = isCarrier ? 7 + outputMag * 4 : 4 + outputMag * 3;
      ofPushStyle();
      ofFill();
      if (isCarrier)
         ofSetColor(255, 220, 150, gModuleDrawAlpha * (.7f + outputMag * .3f));
      else
         ofSetColor(color.r * .5f, color.g * .5f, color.b * .5f, gModuleDrawAlpha * (.5f + outputMag * .3f));
      ofCircle(nx, ny, radius);

      // Ratio label inside/near node
      ofSetColor(255, 255, 255, gModuleDrawAlpha * .4f);
      char ratioStr[8];
      snprintf(ratioStr, sizeof(ratioStr), "%.1f", mOps[i].ratio);
      DrawTextNormal(ratioStr, nx - 8, ny + 3, 8);

      // Operator index
      ofSetColor(255, 255, 255, gModuleDrawAlpha * .2f);
      char idxStr[4];
      snprintf(idxStr, sizeof(idxStr), "%d", i);
      DrawTextNormal(idxStr, nx - 3, ny - radius - 4, 7);

      ofPopStyle();
   }

   // Legend
   ofSetColor(255, 220, 150, gModuleDrawAlpha * .2f);
   DrawTextNormal("carrier", vizX + 5, vizY + vizH - 14, 7);
   ofSetColor(color.r * .5f, color.g * .5f, color.b * .5f, gModuleDrawAlpha * .2f);
   DrawTextNormal("modulator", vizX + 45, vizY + vizH - 14, 7);
   ofSetColor(255, 255, 255, gModuleDrawAlpha * .12f);
   DrawTextNormal("fm cluster", vizX + vizW - 55, vizY + vizH - 5, 8);
}

void FMCluster::OnClicked(float x, float y, bool right)
{
   IDrawableModule::OnClicked(x, y, right);

   // Click an operator node to trigger a note through it
   float vizX = 10, vizY = 172, vizW = 260, vizH = 180;
   float cx = vizX + vizW / 2, cy = vizY + vizH / 2;
   float rad = std::min(vizW, vizH) * 0.32f;

   for (int i = 0; i < mNumOps; ++i)
   {
      float angle = (float)i / mNumOps * FTWO_PI - FPI / 2;
      float nx = cx + cosf(angle) * rad;
      float ny = cy + sinf(angle) * rad;
      float dx = x - nx, dy = y - ny;
      if (dx * dx + dy * dy < 16 * 16)
      {
         // Trigger note with this op as primary carrier
         if (mFrequency < 20) mFrequency = 261.63f;
         mEnvelope.Start(gTime, 0.8f);
         for (int j = 0; j < mNumOps; ++j)
            mOps[j].phase = 0;
         return;
      }
   }
}

void FMCluster::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadString("target", moduleInfo);
   mModuleSaveData.LoadInt("ops", moduleInfo, 4, 2, kFMMaxOps, true);
   SetUpFromSaveData();
}

void FMCluster::SetUpFromSaveData()
{
   SetTarget(TheSynth->FindModule(mModuleSaveData.GetString("target")));
   mNumOps = mModuleSaveData.GetInt("ops");
}
