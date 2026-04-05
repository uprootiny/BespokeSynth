/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/

#include "WavetableMorph.h"
#include "OpenFrameworksPort.h"
#include "SynthGlobals.h"
#include "IAudioReceiver.h"
#include "ModularSynth.h"
#include "Profiler.h"
#include "ofxJSONElement.h"
#include "UIControlMacros.h"
#include "nanovg/nanovg.h"
#include <cmath>

WavetableMorph::WavetableMorph()
: IAudioProcessor(gBufferSize)
, mWriteBuffer(gBufferSize)
{
   mEnvelope.Set(3, 20, 0.7f, 200);
   InitWaveforms();
}

WavetableMorph::~WavetableMorph()
{
}

void WavetableMorph::InitWaveforms()
{
   // Slot 0: Sine
   for (int i = 0; i < kWaveSize; ++i)
      mWaves[0][i] = sinf(FTWO_PI * i / kWaveSize);

   // Slot 1: Triangle
   for (int i = 0; i < kWaveSize; ++i)
   {
      float t = (float)i / kWaveSize;
      mWaves[1][i] = (t < 0.5f) ? (4.0f * t - 1.0f) : (3.0f - 4.0f * t);
   }

   // Slot 2: Saw (bandlimited approximation — first 16 harmonics)
   for (int i = 0; i < kWaveSize; ++i)
   {
      mWaves[2][i] = 0;
      for (int h = 1; h <= 16; ++h)
         mWaves[2][i] += sinf(FTWO_PI * h * i / kWaveSize) / h;
      mWaves[2][i] *= 0.6f;
   }

   // Slot 3: Pulse/Square (bandlimited — odd harmonics)
   for (int i = 0; i < kWaveSize; ++i)
   {
      mWaves[3][i] = 0;
      for (int h = 1; h <= 15; h += 2)
         mWaves[3][i] += sinf(FTWO_PI * h * i / kWaveSize) / h;
      mWaves[3][i] *= 0.8f;
   }
}

void WavetableMorph::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   UIBLOCK0();
   FLOATSLIDER(mMorphSlider, "morph", &mMorph, 0, (float)(kWaveSlots - 1));
   FLOATSLIDER(mDetuneSlider, "detune", &mDetune, -100, 100);
   INTSLIDER(mUnisonSlider, "unison", &mUnisonCount, 1, 4);
   FLOATSLIDER(mUnisonSpreadSlider, "spread", &mUnisonSpread, 0, 0.5f);
   FLOATSLIDER(mVolumeSlider, "vol", &mVolume, 0, 1);
   DROPDOWN(mPresetDropdown, "waves", &mPreset, 70);
   ENDUIBLOCK0();

   mPresetDropdown->AddLabel("classic", 0);
   mPresetDropdown->AddLabel("organ", 1);
   mPresetDropdown->AddLabel("digital", 2);
   mPresetDropdown->AddLabel("warm", 3);

   mEnvDisplay = new ADSRDisplay(this, "env", 3, 110, 140, 20, &mEnvelope);
}

void WavetableMorph::DropdownUpdated(DropdownList* list, int oldVal, double time)
{
   if (list == mPresetDropdown)
   {
      // Different waveform sets per preset
      if (mPreset == 0)
      {
         InitWaveforms(); // classic: sine → tri → saw → square
      }
      else if (mPreset == 1)
      {
         // organ: sine harmonics at different registrations
         for (int i = 0; i < kWaveSize; ++i)
         {
            float ph = FTWO_PI * i / kWaveSize;
            mWaves[0][i] = sinf(ph);                                    // 8'
            mWaves[1][i] = sinf(ph) * 0.7f + sinf(ph * 2) * 0.3f;     // 8' + 4'
            mWaves[2][i] = sinf(ph) * 0.5f + sinf(ph * 2) * 0.3f + sinf(ph * 3) * 0.2f; // 8'+4'+2⅔'
            mWaves[3][i] = sinf(ph) * 0.4f + sinf(ph * 2) * 0.25f + sinf(ph * 3) * 0.15f + sinf(ph * 4) * 0.2f; // full
         }
      }
      else if (mPreset == 2)
      {
         // digital: sharp, metallic waveforms
         for (int i = 0; i < kWaveSize; ++i)
         {
            float t = (float)i / kWaveSize;
            mWaves[0][i] = sinf(FTWO_PI * t);
            mWaves[1][i] = (t < 0.3f) ? 1.0f : -1.0f; // asymmetric pulse
            mWaves[2][i] = sinf(FTWO_PI * t * 5) * (1.0f - t); // decaying harmonics
            mWaves[3][i] = fmodf(t * 7, 1.0f) * 2.0f - 1.0f; // ring-mod like
         }
      }
      else if (mPreset == 3)
      {
         // warm: smooth, filtered waveforms
         for (int i = 0; i < kWaveSize; ++i)
         {
            float ph = FTWO_PI * i / kWaveSize;
            mWaves[0][i] = sinf(ph);
            mWaves[1][i] = sinf(ph) * 0.8f + sinf(ph * 2) * 0.15f + sinf(ph * 3) * 0.05f;
            mWaves[2][i] = sinf(ph) * 0.6f + sinf(ph * 2) * 0.25f + sinf(ph * 4) * 0.1f + sinf(ph * 5) * 0.05f;
            mWaves[3][i] = sinf(ph) * 0.5f + sinf(ph * 3) * 0.3f + sinf(ph * 5) * 0.15f + sinf(ph * 7) * 0.05f;
         }
      }
   }
}

float WavetableMorph::SampleWavetable(float phase, float morph)
{
   // Morph 0-3 crossfades between slots
   morph = ofClamp(morph, 0, (float)(kWaveSlots - 1));
   int slotA = (int)morph;
   int slotB = std::min(slotA + 1, kWaveSlots - 1);
   float blend = morph - slotA;

   // Phase → sample index with linear interpolation
   float fIdx = phase * kWaveSize;
   int idx0 = ((int)fIdx) % kWaveSize;
   int idx1 = (idx0 + 1) % kWaveSize;
   float frac = fIdx - (int)fIdx;

   float sA = mWaves[slotA][idx0] * (1.0f - frac) + mWaves[slotA][idx1] * frac;
   float sB = mWaves[slotB][idx0] * (1.0f - frac) + mWaves[slotB][idx1] * frac;

   return sA * (1.0f - blend) + sB * blend;
}

void WavetableMorph::PlayNote(NoteMessage note)
{
   if (note.velocity > 0)
   {
      mFrequency = 440.0f * powf(2.0f, (note.pitch - 69.0f) / 12.0f);
      mEnvelope.Start(gTime, note.velocity / 127.0f);
      // Spread unison detuning
      for (int u = 0; u < 4; ++u)
      {
         float spread = (u - 1.5f) / 1.5f; // -1 to +1
         mUniDetune[u] = spread * mUnisonSpread;
      }
   }
   else
   {
      mEnvelope.Stop(gTime);
   }
}

void WavetableMorph::Process(double time)
{
   PROFILER(WavetableMorph);

   IAudioReceiver* target = GetTarget();
   if (!mEnabled || target == nullptr)
      return;

   SyncBuffers(1);
   ComputeSliders(0);

   int bufferSize = target->GetBuffer()->BufferSize();
   mWriteBuffer.Clear();
   float* out = mWriteBuffer.GetChannel(0);

   float detuneRatio = powf(2.0f, mDetune / 1200.0f);

   for (int s = 0; s < bufferSize; ++s)
   {
      mEnvValue = mEnvelope.Value(time);
      float sample = 0;

      for (int u = 0; u < mUnisonCount; ++u)
      {
         float freq = mFrequency * detuneRatio * powf(2.0f, mUniDetune[u] / 12.0f);
         float phaseInc = freq / gSampleRate;

         mUniPhase[u] += phaseInc;
         if (mUniPhase[u] > 1.0f) mUniPhase[u] -= 1.0f;

         sample += SampleWavetable(mUniPhase[u], mMorph);
      }

      if (mUnisonCount > 1)
         sample /= sqrtf((float)mUnisonCount); // equal-power normalization

      out[s] = ofClamp(sample * mVolume * mEnvValue, -2.0f, 2.0f);
      time += gInvSampleRateMs;
   }

   GetVizBuffer()->WriteChunk(out, bufferSize, 0);
   Add(target->GetBuffer()->GetChannel(0), out, bufferSize);
   GetBuffer()->Reset();
}

// ============================================================
// VISUALIZATION
// ============================================================

void WavetableMorph::GetModuleDimensions(float& width, float& height)
{
   width = 260;
   height = 280;
}

void WavetableMorph::OnClicked(float x, float y, bool right)
{
   IDrawableModule::OnClicked(x, y, right);

   // Click in wave display to trigger note at C4
   float vizY = 138;
   if (y >= vizY && y <= vizY + 130)
   {
      mFrequency = 261.63f;
      mEnvelope.Start(gTime, 0.8f);
      for (int u = 0; u < 4; ++u)
         mUniDetune[u] = ((u - 1.5f) / 1.5f) * mUnisonSpread;
   }
}

void WavetableMorph::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   mMorphSlider->Draw();
   mDetuneSlider->Draw();
   mUnisonSlider->Draw();
   mUnisonSpreadSlider->Draw();
   mVolumeSlider->Draw();
   mPresetDropdown->Draw();
   mEnvDisplay->Draw();

   // Waveform display: show the morphed waveform + ghost of the two nearest slots
   float vizX = 10, vizY = 138, vizW = 240, vizH = 130;

   // Background
   {
      NVGpaint bg = nvgLinearGradient(gNanoVG, vizX, vizY, vizX, vizY + vizH,
         nvgRGBA(12, 10, 16, (int)(gModuleDrawAlpha * .85f)),
         nvgRGBA(5, 4, 8, (int)(gModuleDrawAlpha * .85f)));
      nvgBeginPath(gNanoVG);
      nvgRoundedRect(gNanoVG, vizX, vizY, vizW, vizH, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, bg);
      nvgFill(gNanoVG);
   }

   ofColor color = GetColor(kModuleCategory_Synth);

   // Center line
   ofSetColor(255, 255, 255, gModuleDrawAlpha * .06f);
   ofSetLineWidth(.5f);
   ofLine(vizX, vizY + vizH / 2, vizX + vizW, vizY + vizH / 2);

   // Ghost: slot A (faint)
   int slotA = ofClamp((int)mMorph, 0, kWaveSlots - 1);
   {
      ofPushStyle();
      ofSetColor(color.r * .3f, color.g * .3f, color.b * .3f, gModuleDrawAlpha * .2f);
      ofSetLineWidth(.5f);
      ofNoFill();
      ofBeginShape();
      for (int i = 0; i < (int)vizW; ++i)
      {
         float t = (float)i / vizW;
         int idx = (int)(t * kWaveSize) % kWaveSize;
         ofVertex(vizX + i, vizY + vizH / 2 - mWaves[slotA][idx] * vizH * 0.4f);
      }
      ofEndShape(false);
      ofPopStyle();
   }

   // Ghost: slot B (faint)
   int slotB = std::min(slotA + 1, kWaveSlots - 1);
   if (slotB != slotA)
   {
      ofPushStyle();
      ofSetColor(color.r * .3f, color.g * .3f, color.b * .3f, gModuleDrawAlpha * .15f);
      ofSetLineWidth(.5f);
      ofNoFill();
      ofBeginShape();
      for (int i = 0; i < (int)vizW; ++i)
      {
         float t = (float)i / vizW;
         int idx = (int)(t * kWaveSize) % kWaveSize;
         ofVertex(vizX + i, vizY + vizH / 2 - mWaves[slotB][idx] * vizH * 0.4f);
      }
      ofEndShape(false);
      ofPopStyle();
   }

   // Morphed waveform (bright, filled)
   {
      nvgBeginPath(gNanoVG);
      nvgMoveTo(gNanoVG, vizX, vizY + vizH / 2);
      for (int i = 0; i < (int)vizW; ++i)
      {
         float t = (float)i / vizW;
         float sample = SampleWavetable(t, mMorph);
         nvgLineTo(gNanoVG, vizX + i, vizY + vizH / 2 - sample * vizH * 0.4f);
      }
      nvgLineTo(gNanoVG, vizX + vizW, vizY + vizH / 2);
      nvgLineTo(gNanoVG, vizX + vizW, vizY + vizH);
      nvgLineTo(gNanoVG, vizX, vizY + vizH);
      nvgClosePath(gNanoVG);
      NVGpaint fill = nvgLinearGradient(gNanoVG, vizX, vizY, vizX, vizY + vizH,
         nvgRGBA(color.r, color.g, color.b, (int)(gModuleDrawAlpha * .2f)),
         nvgRGBA(color.r * .3f, color.g * .3f, color.b * .3f, (int)(gModuleDrawAlpha * .05f)));
      nvgFillPaint(gNanoVG, fill);
      nvgFill(gNanoVG);
   }

   // Morphed stroke
   ofPushStyle();
   ofSetColor(color.r, color.g, color.b, gModuleDrawAlpha * .7f);
   ofSetLineWidth(1.5f);
   ofNoFill();
   ofBeginShape();
   for (int i = 0; i < (int)vizW; ++i)
   {
      float t = (float)i / vizW;
      float sample = SampleWavetable(t, mMorph);
      ofVertex(vizX + i, vizY + vizH / 2 - sample * vizH * 0.4f);
   }
   ofEndShape(false);
   ofPopStyle();

   // Slot indicator dots along bottom
   for (int s = 0; s < kWaveSlots; ++s)
   {
      float dotX = vizX + 10 + (float)s / (kWaveSlots - 1) * (vizW - 20);
      float dotY = vizY + vizH - 8;
      float dist = fabsf(mMorph - s);
      float bright = ofClamp(1.0f - dist, 0, 1);

      ofFill();
      ofSetColor(color.r * (.2f + bright * .8f), color.g * (.2f + bright * .8f),
                 color.b * (.2f + bright * .8f), gModuleDrawAlpha * (.3f + bright * .7f));
      ofCircle(dotX, dotY, 3 + bright * 2);
   }

   // Morph position label
   ofSetColor(255, 255, 255, gModuleDrawAlpha * .15f);
   DrawTextNormal("wavetable morph", vizX + 5, vizY + vizH - 3, 8);
}

void WavetableMorph::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadString("target", moduleInfo);
   SetUpFromSaveData();
}

void WavetableMorph::SetUpFromSaveData()
{
   SetTarget(TheSynth->FindModule(mModuleSaveData.GetString("target")));
}
