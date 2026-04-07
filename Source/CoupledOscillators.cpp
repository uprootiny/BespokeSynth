/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/

#include "CoupledOscillators.h"
#include "OpenFrameworksPort.h"
#include "SynthGlobals.h"
#include "IAudioReceiver.h"
#include "ModularSynth.h"
#include "Profiler.h"
#include "ofxJSONElement.h"
#include "UIControlMacros.h"
#include "nanovg/nanovg.h"
#include <cmath>

CoupledOscillators::CoupledOscillators()
: IAudioProcessor(gBufferSize)
, mWriteBuffer(gBufferSize)
{
   mEnvelope.Set(1, 20, 0.5f, 500); // bell-like: percussive attack, moderate decay, natural ring-out

   // Default frequency ratios: slightly detuned from harmonic
   mMasses[0].freqRatio = 1.0f;
   mMasses[1].freqRatio = 1.007f;   // ~12 cents sharp
   mMasses[2].freqRatio = 2.003f;   // near octave
   mMasses[3].freqRatio = 2.997f;   // near 12th
   mMasses[4].freqRatio = 3.01f;
   mMasses[5].freqRatio = 4.005f;
   mMasses[6].freqRatio = 5.002f;
   mMasses[7].freqRatio = 5.998f;
}

CoupledOscillators::~CoupledOscillators()
{
}

void CoupledOscillators::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   UIBLOCK0();
   INTSLIDER(mNumMassesSlider, "masses", &mNumMasses, 2, kMaxOscCount);
   FLOATSLIDER(mCouplingSlider, "couple", &mCoupling, 0, 0.2f);
   FLOATSLIDER(mSpreadSlider, "spread", &mSpread, 0, 0.2f);
   FLOATSLIDER_DIGITS(mDampingSlider, "sustain", &mDamping, 0.999f, 0.99999f, 5);
   INTSLIDER(mExciteMassSlider, "pluck", &mExciteMass, 0, mNumMasses - 1);
   FLOATSLIDER(mVolumeSlider, "vol", &mVolume, 0, 1);
   ENDUIBLOCK0();

   mEnvDisplay = new ADSRDisplay(this, "env", 3, 105, 130, 20, &mEnvelope);
}

void CoupledOscillators::IntSliderUpdated(IntSlider* slider, int oldVal, double time)
{
   if (slider == mNumMassesSlider)
   {
      mExciteMassSlider->SetExtents(0, mNumMasses - 1);
      if (mExciteMass >= mNumMasses) mExciteMass = 0;

      // Recompute frequency ratios with spread
      for (int i = 0; i < mNumMasses; ++i)
         mMasses[i].freqRatio = (i + 1) * (1.0f + (i * mSpread));
   }
}

void CoupledOscillators::PlayNote(NoteMessage note)
{
   if (note.velocity > 0)
   {
      mFrequency = 440.0f * powf(2.0f, (note.pitch - 69.0f) / 12.0f);
      mEnvelope.Start(gTime, note.velocity / 127.0f);

      // Recompute frequency ratios
      for (int i = 0; i < mNumMasses; ++i)
         mMasses[i].freqRatio = (i + 1) * (1.0f + (i * mSpread));

      // Reset all masses
      for (int i = 0; i < mNumMasses; ++i)
      {
         mMasses[i].pos = 0;
         mMasses[i].vel = 0;
      }

      // Pluck the excitation mass
      if (mExciteMass >= 0 && mExciteMass < mNumMasses)
         mMasses[mExciteMass].vel = note.velocity / 127.0f * 0.5f;
   }
   else
   {
      mEnvelope.Stop(gTime);
   }
}

void CoupledOscillators::Process(double time)
{
   PROFILER(CoupledOscillators);

   IAudioReceiver* target = GetTarget();
   if (!mEnabled || target == nullptr)
      return;

   SyncBuffers(1);
   ComputeSliders(0);

   int bufferSize = target->GetBuffer()->BufferSize();
   mWriteBuffer.Clear();
   float* out = mWriteBuffer.GetChannel(0);

   // Precompute spring constants AND per-mass damping (once per buffer)
   float omega2[kMaxOscCount];
   float massDamp[kMaxOscCount];
   float maxFreq = gSampleRate * 0.25f;
   for (int i = 0; i < mNumMasses; ++i)
   {
      float f = std::min(mFrequency * mMasses[i].freqRatio, maxFreq);
      float w = FTWO_PI * f / gSampleRate;
      omega2[i] = w * w;
      // Frequency-dependent damping: Qf = constant for metallic resonators
      // Precomputed here, not in the inner loop (powf is expensive)
      massDamp[i] = powf(mDamping, 1.0f + 0.3f * log2f(std::max(0.25f, mMasses[i].freqRatio)));
   }

   for (int s = 0; s < bufferSize; ++s)
   {
      mEnvValue = mEnvelope.Value(time);

      for (int i = 0; i < mNumMasses; ++i)
      {
         float accel = -omega2[i] * mMasses[i].pos;

         for (int j = 0; j < mNumMasses; ++j)
         {
            if (j != i)
               accel += mCoupling * (mMasses[j].pos - mMasses[i].pos);
         }

         mMasses[i].vel += accel;
         mMasses[i].vel *= massDamp[i]; // precomputed, no transcendentals here
         mMasses[i].pos += mMasses[i].vel;
      }

      // Output: sum of all mass displacements
      float sample = 0;
      for (int i = 0; i < mNumMasses; ++i)
         sample += mMasses[i].pos;
      sample /= mNumMasses;

      out[s] = ofClamp(sample * mVolume * mEnvValue, -1.0f, 1.0f); if (!std::isfinite(out[s])) out[s] = 0;
      time += gInvSampleRateMs;
   }

   // Store viz state
   for (int i = 0; i < mNumMasses; ++i)
      mMasses[i].prevPos = mMasses[i].pos;

   GetVizBuffer()->WriteChunk(out, bufferSize, 0);
   Add(target->GetBuffer()->GetChannel(0), out, bufferSize);
   GetBuffer()->Reset();
}

// ============================================================
// VISUALIZATION
// ============================================================

void CoupledOscillators::GetModuleDimensions(float& width, float& height)
{
   width = 240;
   height = 270;
}

void CoupledOscillators::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   mNumMassesSlider->Draw();
   mCouplingSlider->Draw();
   mSpreadSlider->Draw();
   mDampingSlider->Draw();
   mExciteMassSlider->Draw();
   mVolumeSlider->Draw();
   mEnvDisplay->Draw();

   // Physics visualization
   float vizX = 5, vizY = 130, vizW = 230, vizH = 130;
   float cx = vizX + vizW / 2;

   // Dark background
   {
      NVGpaint bg = nvgLinearGradient(gNanoVG, vizX, vizY, vizX, vizY + vizH,
         nvgRGBA(12, 10, 18, (int)(gModuleDrawAlpha * .85f)),
         nvgRGBA(5, 4, 8, (int)(gModuleDrawAlpha * .85f)));
      nvgBeginPath(gNanoVG);
      nvgRoundedRect(gNanoVG, vizX, vizY, vizW, vizH, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, bg);
      nvgFill(gNanoVG);
   }

   ofColor color = GetColor(kModuleCategory_Synth);

   // Arrange masses horizontally, displacement shown vertically
   float massSpacing = vizW / (mNumMasses + 1);
   float restY = vizY + vizH / 2;

   for (int i = 0; i < mNumMasses; ++i)
   {
      float mx = vizX + massSpacing * (i + 1);
      float displacement = mMasses[i].pos * 300; // scale for visibility
      displacement = ofClamp(displacement, -vizH / 2 + 10, vizH / 2 - 10);
      float my = restY + displacement;

      float energy = mMasses[i].pos * mMasses[i].pos + mMasses[i].vel * mMasses[i].vel;
      float mag = ofClamp(sqrtf(energy) * 20, 0, 1);

      // Spring to left neighbor
      if (i > 0)
      {
         float prevX = vizX + massSpacing * i;
         float prevDisp = mMasses[i - 1].pos * 300;
         prevDisp = ofClamp(prevDisp, -vizH / 2 + 10, vizH / 2 - 10);
         float prevY = restY + prevDisp;

         // Spring as zigzag
         float springStretch = fabsf(my - prevY) / vizH;
         float springMag = ofClamp(springStretch * 5, 0, 1);

         ofPushStyle();
         // Color: blue when compressed, red when stretched
         float r = 80 + 175 * springMag;
         float g = 80;
         float b = 255 - 175 * springMag;
         ofSetColor(r, g, b, gModuleDrawAlpha * (.3f + springMag * .4f));
         ofSetLineWidth(1 + springMag);

         // Draw zigzag spring
         int zigzags = 6;
         float dx = (mx - prevX) / (zigzags * 2 + 2);
         float amp = 4 * (1 + springMag * 2);
         ofBeginShape();
         ofVertex(prevX, prevY);
         for (int z = 0; z < zigzags * 2; ++z)
         {
            float t = (float)(z + 1) / (zigzags * 2 + 1);
            float sx = prevX + (mx - prevX) * t;
            float sy = prevY + (my - prevY) * t + ((z % 2 == 0) ? amp : -amp);
            ofVertex(sx, sy);
         }
         ofVertex(mx, my);
         ofEndShape(false);
         ofPopStyle();
      }

      // Mass glow
      if (mag > 0.01f)
      {
         NVGpaint glow = nvgRadialGradient(gNanoVG, mx, my, 3, 16,
            nvgRGBA(color.r, color.g, color.b, (int)(mag * gModuleDrawAlpha * .4f)),
            nvgRGBA(color.r, color.g, color.b, 0));
         nvgBeginPath(gNanoVG);
         nvgCircle(gNanoVG, mx, my, 20);
         nvgFillPaint(gNanoVG, glow);
         nvgFill(gNanoVG);
      }

      // Mass circle
      float radius = 5 + mag * 4;
      ofPushStyle();
      ofFill();
      ofSetColor(color.r * (.4f + mag * .6f), color.g * (.4f + mag * .6f),
                 color.b * (.4f + mag * .6f), gModuleDrawAlpha * (.7f + mag * .3f));
      ofCircle(mx, my, radius);

      // Excite marker
      if (i == mExciteMass)
      {
         ofNoFill();
         ofSetColor(255, 255, 255, gModuleDrawAlpha * .4f);
         ofSetLineWidth(1);
         ofCircle(mx, my, radius + 4);
      }
      ofPopStyle();

      // Rest position line (faint)
      ofSetColor(255, 255, 255, gModuleDrawAlpha * .06f);
      ofSetLineWidth(.5f);
      ofLine(mx, restY - vizH / 2 + 5, mx, restY + vizH / 2 - 5);
   }

   // Label
   ofSetColor(255, 255, 255, gModuleDrawAlpha * .15f);
   DrawTextNormal("coupled oscillators", vizX + 5, vizY + vizH - 4, 8);
}

void CoupledOscillators::OnClicked(float x, float y, bool right)
{
   IDrawableModule::OnClicked(x, y, right);

   // Hit-test masses in the visualization area
   float vizX = 5, vizY = 130, vizW = 230, vizH = 130;
   float restY = vizY + vizH / 2;
   float massSpacing = vizW / (mNumMasses + 1);

   for (int i = 0; i < mNumMasses; ++i)
   {
      float mx = vizX + massSpacing * (i + 1);
      float displacement = ofClamp(mMasses[i].pos * 300, -vizH / 2 + 10, vizH / 2 - 10);
      float my = restY + displacement;

      float dx = x - mx, dy = y - my;
      if (dx * dx + dy * dy < 18 * 18)
      {
         // Pluck this mass
         if (mFrequency < 20) mFrequency = 261.63f;
         mMasses[i].vel += 0.4f;
         mEnvelope.Start(gTime, 0.8f);
         mExciteMass = i;
         return;
      }
   }
}

void CoupledOscillators::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadString("target", moduleInfo);
   mModuleSaveData.LoadInt("masses", moduleInfo, 4, 2, kMaxOscCount, true);
   SetUpFromSaveData();
}

void CoupledOscillators::SetUpFromSaveData()
{
   SetTarget(TheSynth->FindModule(mModuleSaveData.GetString("target")));
   mNumMasses = mModuleSaveData.GetInt("masses");
}
