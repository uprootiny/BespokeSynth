/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/

#include "FibonacciComb.h"
#include "OpenFrameworksPort.h"
#include "SynthGlobals.h"
#include "IAudioReceiver.h"
#include "ModularSynth.h"
#include "Profiler.h"
#include "ofxJSONElement.h"
#include "UIControlMacros.h"
#include "nanovg/nanovg.h"
#include <cmath>

FibonacciComb::FibonacciComb()
: IAudioProcessor(gBufferSize)
{
   // Precompute Fibonacci sequence
   mFib[0] = 1; mFib[1] = 1;
   for (int i = 2; i < kFibMaxTaps; ++i)
      mFib[i] = mFib[i - 1] + mFib[i - 2];
   // Sequence: 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377
}

FibonacciComb::~FibonacciComb()
{
}

void FibonacciComb::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   UIBLOCK0();
   FLOATSLIDER(mFundSlider, "freq", &mFundamental, 30, 2000);
   INTSLIDER(mDepthSlider, "taps", &mDepth, 2, kFibMaxTaps);
   FLOATSLIDER(mFeedbackSlider, "feedback", &mFeedback, 0, 0.95f);
   FLOATSLIDER(mDampingSlider, "damp", &mDamping, 0, 0.95f);
   FLOATSLIDER(mWetDrySlider, "mix", &mWetDry, 0, 1);
   ENDUIBLOCK0();
}

void FibonacciComb::Process(double time)
{
   PROFILER(FibonacciComb);

   IAudioReceiver* target = GetTarget();
   if (!mEnabled || target == nullptr)
      return;

   SyncBuffers();
   ComputeSliders(0);

   int bufferSize = GetBuffer()->BufferSize();
   float* input = GetBuffer()->GetChannel(0);

   // Base delay in samples from fundamental frequency
   float baseDelay = gSampleRate / std::max(mFundamental, 20.0f);

   for (int s = 0; s < bufferSize; ++s)
   {
      float dry = input[s];
      float wet = 0;

      // Sum Fibonacci-positioned taps
      for (int t = 0; t < mDepth; ++t)
      {
         int delaySamples = (int)(mFib[t] * baseDelay);
         delaySamples = std::max(1, std::min(delaySamples, kFibMaxDelay - 1));

         int readPos = (mWritePos - delaySamples + kFibMaxDelay) % kFibMaxDelay;
         float tapSample = mBuf[readPos];

         // Per-tap HF damping: one-pole lowpass
         mDampState[t] = (1.0f - mDamping) * tapSample + mDamping * mDampState[t];
         tapSample = mDampState[t];

         // Fibonacci-weighted gain: each tap decays by feedback^tap_index
         float tapGain = powf(mFeedback, (float)t);
         wet += tapSample * tapGain;

         // Store for viz
         if (s == bufferSize / 2)
            mTapViz[t] = fabsf(tapSample * tapGain);
      }

      // Normalize
      if (mDepth > 0)
         wet /= sqrtf((float)mDepth);

      // Write to delay buffer (input + feedback)
      mBuf[mWritePos] = dry + wet * mFeedback * 0.5f;
      mWritePos = (mWritePos + 1) % kFibMaxDelay;

      input[s] = ofClamp(dry * (1.0f - mWetDry) + wet * mWetDry, -4.0f, 4.0f);
   }

   for (int ch = 0; ch < GetBuffer()->NumActiveChannels(); ++ch)
   {
      Add(target->GetBuffer()->GetChannel(ch), GetBuffer()->GetChannel(ch), bufferSize);
      GetVizBuffer()->WriteChunk(GetBuffer()->GetChannel(ch), bufferSize, ch);
   }
   GetBuffer()->Reset();
}

// ============================================================
// VISUALIZATION — Golden Spiral
// ============================================================

void FibonacciComb::GetModuleDimensions(float& width, float& height)
{
   width = 220;
   height = 300;
}

void FibonacciComb::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   mFundSlider->Draw();
   mDepthSlider->Draw();
   mFeedbackSlider->Draw();
   mDampingSlider->Draw();
   mWetDrySlider->Draw();

   // Golden spiral visualization
   float vizX = 10, vizY = 95, vizW = 200, vizH = 195;
   float cx = vizX + vizW / 2, cy = vizY + vizH / 2;

   // Background
   {
      NVGpaint bg = nvgRadialGradient(gNanoVG, cx, cy, 5, vizW * 0.5f,
         nvgRGBA(10, 12, 16, (int)(gModuleDrawAlpha * .85f)),
         nvgRGBA(3, 4, 6, (int)(gModuleDrawAlpha * .9f)));
      nvgBeginPath(gNanoVG);
      nvgRoundedRect(gNanoVG, vizX, vizY, vizW, vizH, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, bg);
      nvgFill(gNanoVG);
   }

   ofColor color = GetColor(kModuleCategory_Audio);

   // Draw golden spiral with taps as dots
   // The golden angle: 2π / φ² ≈ 137.5°
   float goldenAngle = FTWO_PI / (1.6180339887f * 1.6180339887f);
   float maxRadius = std::min(vizW, vizH) * 0.42f;

   // Spiral path
   ofPushStyle();
   ofNoFill();
   ofSetColor(color.r * .2f, color.g * .2f, color.b * .2f, gModuleDrawAlpha * .2f);
   ofSetLineWidth(0.5f);
   ofBeginShape();
   for (int i = 0; i < mDepth * 8; ++i)
   {
      float t = (float)i / (mDepth * 8 - 1);
      float r = maxRadius * sqrtf(t);
      float angle = i * goldenAngle;
      ofVertex(cx + cosf(angle) * r, cy + sinf(angle) * r);
   }
   ofEndShape(false);
   ofPopStyle();

   // Tap dots on the spiral
   for (int t = 0; t < mDepth; ++t)
   {
      float spiralT = (float)(t + 1) / mDepth;
      float r = maxRadius * sqrtf(spiralT);
      float angle = (t + 1) * goldenAngle;
      float tx = cx + cosf(angle) * r;
      float ty = cy + sinf(angle) * r;

      float mag = ofClamp(mTapViz[t] * 8, 0, 1);

      // Tap glow
      if (mag > 0.01f)
      {
         // Color: warm for early taps (low Fibonacci), cool for late (high Fibonacci)
         float tapWarmth = 1.0f - spiralT;
         int cr = (int)(255 * tapWarmth + 80 * (1 - tapWarmth));
         int cg = (int)(180 * tapWarmth + 180 * (1 - tapWarmth));
         int cb = (int)(80 * tapWarmth + 255 * (1 - tapWarmth));

         NVGpaint glow = nvgRadialGradient(gNanoVG, tx, ty, 2, 12,
            nvgRGBA(cr, cg, cb, (int)(mag * gModuleDrawAlpha * .5f)),
            nvgRGBA(cr, cg, cb, 0));
         nvgBeginPath(gNanoVG);
         nvgCircle(gNanoVG, tx, ty, 14);
         nvgFillPaint(gNanoVG, glow);
         nvgFill(gNanoVG);

         ofFill();
         ofSetColor(cr, cg, cb, (int)(mag * gModuleDrawAlpha * .8f));
         ofCircle(tx, ty, 3 + mag * 3);
      }
      else
      {
         ofFill();
         ofSetColor(color.r * .3f, color.g * .3f, color.b * .3f, gModuleDrawAlpha * .3f);
         ofCircle(tx, ty, 2);
      }

      // Fibonacci number label (small)
      ofSetColor(255, 255, 255, gModuleDrawAlpha * .15f);
      char fibStr[8];
      snprintf(fibStr, sizeof(fibStr), "%d", mFib[t]);
      DrawTextNormal(fibStr, tx + 6, ty + 3, 7);
   }

   // Golden ratio label
   ofSetColor(255, 220, 150, gModuleDrawAlpha * .15f);
   DrawTextNormal("phi = 1.618...", vizX + 5, vizY + vizH - 14, 8);
   ofSetColor(255, 255, 255, gModuleDrawAlpha * .1f);
   DrawTextNormal("fibonacci comb", vizX + vizW - 80, vizY + vizH - 5, 8);
}

void FibonacciComb::LoadLayout(const ofxJSONElement& moduleInfo)
{
   SetUpFromSaveData();
}

void FibonacciComb::SetUpFromSaveData()
{
}
