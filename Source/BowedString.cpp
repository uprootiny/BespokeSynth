/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/

#include "BowedString.h"
#include "OpenFrameworksPort.h"
#include "SynthGlobals.h"
#include "IAudioReceiver.h"
#include "ModularSynth.h"
#include "Profiler.h"
#include "ofxJSONElement.h"
#include "UIControlMacros.h"
#include "nanovg/nanovg.h"
#include <cmath>

// Published violin body mode data (simplified from Bissinger 2008)
static const float kDefaultBodyModes[kBodyModes][3] = {
   // { frequency, Q, gain }
   { 275, 15, 1.0f },    // A0: air mode (Helmholtz cavity resonance)
   { 390, 20, 0.6f },    // B1-: lower body mode
   { 460, 25, 0.8f },    // T1: top plate first bending
   { 510, 20, 0.7f },    // B1+: upper body mode
   { 700, 30, 0.4f },    // Higher body mode
   { 1100, 40, 0.3f },   // "Bridge hill" region
};

BowedString::BowedString()
: IAudioProcessor(gBufferSize)
, mWriteBuffer(gBufferSize)
, mStringViz(512)
{
   for (int i = 0; i < kBodyModes; ++i)
   {
      mBodyModes[i].freq = kDefaultBodyModes[i][0];
      mBodyModes[i].q = kDefaultBodyModes[i][1];
      mBodyModes[i].gain = kDefaultBodyModes[i][2];
   }
}

BowedString::~BowedString()
{
}

void BowedString::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   UIBLOCK0();
   FLOATSLIDER(mBowVelSlider, "bow vel", &mBowVelocity, 0, 0.8f);
   FLOATSLIDER(mBowPressSlider, "pressure", &mBowPressure, 0.05f, 1.0f);
   FLOATSLIDER(mBowPosSlider, "bow pos", &mBowPosition, 0.03f, 0.3f);
   FLOATSLIDER(mBowNoiseSlider, "rosin", &mBowNoise, 0, 0.1f);
   FLOATSLIDER_DIGITS(mDampingSlider, "sustain", &mStringDamping, 0.999f, 0.99999f, 5);
   FLOATSLIDER(mBrightnessSlider, "bright", &mBrightness, 0, 1);
   FLOATSLIDER(mBodySizeSlider, "body", &mBodySize, 0.5f, 2.0f);
   FLOATSLIDER(mBodyResSlider, "body Q", &mBodyResonance, 0.3f, 0.99f);
   FLOATSLIDER(mSympSlider, "sympath", &mSympCoupling, 0, 0.2f);
   INTSLIDER(mNumStringsSlider, "strings", &mNumStrings, 1, kMaxStrings);
   FLOATSLIDER(mVolumeSlider, "vol", &mVolume, 0, 1);
   ENDUIBLOCK0();
}

void BowedString::UpdateStringLengths(int idx)
{
   if (idx < 0 || idx >= kMaxStrings) return;
   float freq = mStrings[idx].frequency;
   if (freq < 20) freq = 20;

   // Total delay = sr/freq. Split at bow position.
   float totalDelay = gSampleRate / freq;
   float bridgeFrac = mBowPosition;
   float nutFrac = 1.0f - bridgeFrac;

   mStrings[idx].bridgeLen = std::max(2, std::min((int)(totalDelay * bridgeFrac), kBowedDelayMax - 1));
   mStrings[idx].nutLen = std::max(2, std::min((int)(totalDelay * nutFrac), kBowedDelayMax - 1));
}

float BowedString::AllpassRead(float* buf, int wp, int len, float delay, float& ap)
{
   if (len <= 1) return buf[0];
   int intD = (int)delay;
   float frac = delay - intD;
   int idx = (wp - intD + len * 2) % len;
   if (frac < 0.001f) { ap = buf[idx]; return buf[idx]; }
   float a = (1.0f - frac) / (1.0f + frac);
   int idxP = (idx - 1 + len) % len;
   float y = a * buf[idx] + buf[idxP] - a * ap;
   ap = y;
   return y;
}

float BowedString::ComputeFriction(float vRel)
{
   // Hyperbolic friction: mu(v) = mu_d + (mu_s - mu_d) * v_break / (|v| + v_break)
   const float mu_s = 0.8f;   // static friction
   const float mu_d = 0.3f;   // dynamic friction
   const float v_break = 0.01f; // breakaway velocity

   float absV = fabsf(vRel) + 1e-10f;
   float mu = mu_d + (mu_s - mu_d) * v_break / (absV + v_break);

   // Friction force = mu * f_N * sign(v_rel)
   float sign = (vRel > 0) ? 1.0f : -1.0f;
   return mu * mBowPressure * sign;
}

float BowedString::ProcessBodyMode(BodyMode& mode, float input)
{
   // Biquad bandpass with frequency-scaled Q
   // Physical: wooden resonators have Q proportional to sqrt(f/f_ref)
   // Higher body modes ring slightly longer relative to their period
   float scaledFreq = mode.freq * mBodySize;
   float w0 = FTWO_PI * scaledFreq / gSampleRate;
   if (w0 > FPI * 0.95f) w0 = FPI * 0.95f;
   float qScale = sqrtf(scaledFreq / 275.0f); // normalize to A0 mode
   float effectiveQ = mode.q * mBodyResonance * std::max(0.5f, std::min(qScale, 2.0f));
   float alpha = sinf(w0) / (2.0f * effectiveQ);

   float a0 = 1.0f + alpha;
   float b0 = alpha / a0;
   float b2 = -alpha / a0;
   float a1 = (-2.0f * cosf(w0)) / a0;
   float a2 = (1.0f - alpha) / a0;

   float y = b0 * input + b2 * (-mode.z2) - a1 * mode.z1 - a2 * mode.z2;
   mode.z2 = mode.z1;
   mode.z1 = y;
   return y * mode.gain;
}

void BowedString::PlayNote(NoteMessage note)
{
   if (note.velocity > 0)
   {
      float freq = 440.0f * powf(2.0f, (note.pitch - 69.0f) / 12.0f);
      mStrings[0].frequency = freq;
      mStrings[0].active = true;
      UpdateStringLengths(0);
      mBowing = true;

      // Set sympathetic string tunings
      for (int i = 1; i < mNumStrings; ++i)
      {
         mStrings[i].frequency = freq * powf(2.0f, mSympTuning[i] / 12.0f);
         mStrings[i].active = true;
         UpdateStringLengths(i);
      }
   }
   else
   {
      mBowing = false;
   }
}

void BowedString::Process(double time)
{
   PROFILER(BowedString);

   IAudioReceiver* target = GetTarget();
   if (!mEnabled || target == nullptr)
      return;

   SyncBuffers(1);
   ComputeSliders(0);

   int bufferSize = target->GetBuffer()->BufferSize();
   mWriteBuffer.Clear();
   float* out = mWriteBuffer.GetChannel(0);

   for (int s = 0; s < bufferSize; ++s)
   {
      float totalOut = 0;

      for (int str = 0; str < mNumStrings; ++str)
      {
         if (!mStrings[str].active) continue;
         auto& st = mStrings[str];

         // Read incoming waves at bow point
         float fromNut = AllpassRead(st.delayNut, st.writeNut, st.nutLen, st.nutLen - 1, st.apNut);
         float fromBridge = AllpassRead(st.delayBridge, st.writeBridge, st.bridgeLen, st.bridgeLen - 1, st.apBridge);

         // String velocity at bow point = sum of incoming waves
         float vIncoming = fromNut + fromBridge;

         // Bow excitation (only on main string, or all if coupling > 0)
         float friction = 0;
         if (mBowing && (str == 0 || mSympCoupling > 0.1f))
         {
            float bowVel = mBowVelocity;
            // Add bow noise (rosin texture)
            bowVel += mBowNoise * RandomSample();

            float vRel = bowVel - vIncoming;
            friction = ComputeFriction(vRel);

            if (str > 0)
               friction *= mSympCoupling; // sympathetic strings get weak excitation
         }

         // Outgoing waves from bow point
         float outToNut = fromBridge + friction;
         float outToBridge = fromNut + friction;

         // Damping
         outToNut *= mStringDamping;
         outToBridge *= mStringDamping;

         // Brightness: simple one-pole lowpass on the bridge-side wave
         float lpCoeff = 0.2f + mBrightness * 0.8f;
         outToBridge = lpCoeff * outToBridge + (1.0f - lpCoeff) * st.lastFriction;
         st.lastFriction = outToBridge;

         // Write into delay lines
         st.delayNut[st.writeNut] = outToNut;
         st.delayBridge[st.writeBridge] = outToBridge;
         st.writeNut = (st.writeNut + 1) % std::max(2, st.nutLen);
         st.writeBridge = (st.writeBridge + 1) % std::max(2, st.bridgeLen);

         // Bridge output: the wave arriving at the bridge end
         totalOut += outToBridge;

         // Store for viz
         st.vString = vIncoming;
      }

      totalOut /= std::max(1, mNumStrings);

      // Bridge filter: biquad highpass at ~3kHz
      {
         float w0 = FTWO_PI * 3000.0f / gSampleRate;
         float alpha = sinf(w0) / 1.414f; // Q = sqrt(2)/2
         float a0 = 1.0f + alpha;
         float b0 = ((1.0f + cosf(w0)) / 2.0f) / a0;
         float b1 = (-(1.0f + cosf(w0))) / a0;
         float b2 = b0;
         float a1 = (-2.0f * cosf(w0)) / a0;
         float a2 = (1.0f - alpha) / a0;

         float bridgeOut = b0 * totalOut + b1 * mBridgeZ1 + b2 * mBridgeZ2
                          - a1 * mBridgeZ1 - a2 * mBridgeZ2;
         mBridgeZ2 = mBridgeZ1;
         mBridgeZ1 = bridgeOut;

         // Mix: direct string + bridge-filtered (the bridge boosts upper harmonics)
         totalOut = totalOut * 0.4f + bridgeOut * 0.6f;
      }

      // Body resonator: sum of parallel bandpass modes
      float bodyOut = 0;
      for (int m = 0; m < kBodyModes; ++m)
         bodyOut += ProcessBodyMode(mBodyModes[m], totalOut);

      // Mix body + direct
      float finalOut = totalOut * 0.2f + bodyOut * 0.8f;

      out[s] = ofClamp(finalOut * mVolume, -1.0f, 1.0f); if (!std::isfinite(out[s])) out[s] = 0;

      mStringViz.Write(mStrings[0].vString, 0);
      mBowContactForce = fabsf(mStrings[0].lastFriction);

      time += gInvSampleRateMs;
   }

   GetVizBuffer()->WriteChunk(out, bufferSize, 0);
   Add(target->GetBuffer()->GetChannel(0), out, bufferSize);
   GetBuffer()->Reset();
}

// ============================================================
// VISUALIZATION
// ============================================================

void BowedString::GetModuleDimensions(float& width, float& height)
{
   width = 310;
   height = 340;
}

void BowedString::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   // Controls
   mBowVelSlider->Draw();
   mBowPressSlider->Draw();
   mBowPosSlider->Draw();
   mBowNoiseSlider->Draw();
   mDampingSlider->Draw();
   mBrightnessSlider->Draw();
   mBodySizeSlider->Draw();
   mBodyResSlider->Draw();
   mSympSlider->Draw();
   mNumStringsSlider->Draw();
   mVolumeSlider->Draw();

   // === String visualization ===
   float vizX = 5, vizY = 185, vizW = 300, vizH = 145;

   // Background
   {
      NVGpaint bg = nvgLinearGradient(gNanoVG, vizX, vizY, vizX, vizY + vizH,
         nvgRGBA(15, 12, 10, (int)(gModuleDrawAlpha * .85f)),
         nvgRGBA(6, 5, 4, (int)(gModuleDrawAlpha * .9f)));
      nvgBeginPath(gNanoVG);
      nvgRoundedRect(gNanoVG, vizX, vizY, vizW, vizH, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, bg);
      nvgFill(gNanoVG);
   }

   ofColor color = GetColor(kModuleCategory_Synth);

   // Draw each string
   float stringSpacing = vizH / (mNumStrings + 1);
   for (int str = 0; str < mNumStrings; ++str)
   {
      float sy = vizY + stringSpacing * (str + 1);
      float alpha = (str == 0) ? 1.0f : 0.4f; // main string brighter

      // Nut and bridge endpoints
      ofSetColor(200, 180, 150, gModuleDrawAlpha * .5f * alpha);
      ofFill();
      ofRect(vizX + 5, sy - 3, 3, 6, 1);   // nut
      ofRect(vizX + vizW - 8, sy - 3, 3, 6, 1); // bridge

      // Bow position marker
      float bowX = vizX + 8 + mBowPosition * (vizW - 16);
      float bowForce = (str == 0) ? mBowContactForce * 50 : mBowContactForce * 50 * mSympCoupling;
      bowForce = ofClamp(bowForce, 0, 1);

      // String waveform
      if (str == 0 && mStringViz.Size() > 0)
      {
         ofSetColor(color.r, color.g, color.b, gModuleDrawAlpha * .7f * alpha);
         ofSetLineWidth(1.5f);
         ofNoFill();
         ofBeginShape();
         int vizSamples = std::min(mStringViz.Size(), (int)vizW - 16);
         if (vizSamples < 2) vizSamples = 2; // prevent division by zero
         for (int i = 0; i < vizSamples; ++i)
         {
            float sample = mStringViz.GetSample(vizSamples - 1 - i, 0);
            float x = vizX + 8 + (float)i / (vizSamples - 1) * (vizW - 16);
            float y = sy + sample * stringSpacing * 0.4f;
            ofVertex(x, y);
         }
         ofEndShape(false);
      }
      else
      {
         // Resting string
         ofSetColor(color.r * .3f, color.g * .3f, color.b * .3f, gModuleDrawAlpha * .3f * alpha);
         ofSetLineWidth(1);
         ofLine(vizX + 8, sy, vizX + vizW - 8, sy);
      }

      // Bow contact (vertical line with glow)
      if (mBowing)
      {
         NVGpaint bowGlow = nvgRadialGradient(gNanoVG, bowX, sy, 1, 8,
            nvgRGBA(255, 220, 180, (int)(bowForce * gModuleDrawAlpha * .4f)),
            nvgRGBA(255, 220, 180, 0));
         nvgBeginPath(gNanoVG);
         nvgCircle(gNanoVG, bowX, sy, 10);
         nvgFillPaint(gNanoVG, bowGlow);
         nvgFill(gNanoVG);

         ofSetColor(255, 220, 180, gModuleDrawAlpha * (.3f + bowForce * .5f));
         ofSetLineWidth(1 + bowForce * 2);
         ofLine(bowX, sy - stringSpacing * 0.3f, bowX, sy + stringSpacing * 0.3f);
      }
   }

   // Body mode bars (bottom strip)
   {
      float barY = vizY + vizH - 16;
      float barH = 10;
      float barW = vizW / kBodyModes;

      for (int m = 0; m < kBodyModes; ++m)
      {
         float energy = fabsf(mBodyModes[m].z1) * 50;
         float mag = ofClamp(energy, 0, 1);

         float x = vizX + m * barW + 2;
         float w = barW - 4;

         // Bar background
         nvgBeginPath(gNanoVG);
         nvgRoundedRect(gNanoVG, x, barY, w, barH, 2);
         nvgFillColor(gNanoVG, nvgRGBA(20, 18, 15, (int)(gModuleDrawAlpha * .5f)));
         nvgFill(gNanoVG);

         // Bar fill
         if (mag > 0.01f)
         {
            nvgBeginPath(gNanoVG);
            nvgRoundedRect(gNanoVG, x, barY, w * mag, barH, 2);
            nvgFillColor(gNanoVG, nvgRGBA(
               (int)(200 + 55 * mag), (int)(150 * mag), (int)(80 * mag),
               (int)(mag * gModuleDrawAlpha * .6f)));
            nvgFill(gNanoVG);
         }
      }

      ofSetColor(255, 255, 255, gModuleDrawAlpha * .15f);
      DrawTextNormal("body modes", vizX + 5, barY - 2, 7);
   }

   ofSetColor(255, 255, 255, gModuleDrawAlpha * .12f);
   DrawTextNormal("bowed string", vizX + vizW - 70, vizY + vizH - 4, 8);
}

void BowedString::OnClicked(float x, float y, bool right)
{
   IDrawableModule::OnClicked(x, y, right);

   // Click on the string area to start bowing
   float vizX = 5, vizY = 185, vizW = 300, vizH = 145;
   if (x >= vizX && x <= vizX + vizW && y >= vizY && y <= vizY + vizH)
   {
      // X position maps to bow position (0=nut, 1=bridge)
      float clickPos = (x - vizX - 8) / (vizW - 16);
      mBowPosition = ofClamp(clickPos, 0.03f, 0.3f);

      // Start bowing if not already
      if (!mBowing)
      {
         if (mStrings[0].frequency < 20) mStrings[0].frequency = 261.63f;
         mStrings[0].active = true;
         UpdateStringLengths(0);
         mBowing = true;
      }
   }
}

void BowedString::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadString("target", moduleInfo);
   SetUpFromSaveData();
}

void BowedString::SetUpFromSaveData()
{
   SetTarget(TheSynth->FindModule(mModuleSaveData.GetString("target")));
}
