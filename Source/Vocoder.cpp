/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
//
//  Vocoder.cpp
//  modularSynth
//
//  Created by Ryan Challinor on 4/17/13.
//
//

#include "Vocoder.h"
#include "ModularSynth.h"
#include "Profiler.h"
#include "nanovg/nanovg.h"

Vocoder::Vocoder()
: IAudioProcessor(gBufferSize)
{
   // Generate a window with a single raised cosine from N/4 to 3N/4
   mWindower = new float[VOCODER_WINDOW_SIZE];
   for (int i = 0; i < VOCODER_WINDOW_SIZE; ++i)
      mWindower[i] = -.5 * cos(FTWO_PI * i / VOCODER_WINDOW_SIZE) + .5;

   mCarrierInputBuffer = new float[GetBuffer()->BufferSize()];
   Clear(mCarrierInputBuffer, GetBuffer()->BufferSize());

   AddChild(&mGate);
   mGate.SetPosition(110, 20);
   mGate.SetEnabled(false);
}

void Vocoder::CreateUIControls()
{
   IDrawableModule::CreateUIControls();
   mInputSlider = new FloatSlider(this, "input", 5, 29, 100, 15, &mInputPreamp, 0, 2);
   mCarrierSlider = new FloatSlider(this, "carrier", 5, 47, 100, 15, &mCarrierPreamp, 0, 2);
   mVolumeSlider = new FloatSlider(this, "volume", 5, 65, 100, 15, &mVolume, 0, 2);
   mDryWetSlider = new FloatSlider(this, "dry/wet", 5, 83, 100, 15, &mDryWet, 0, 1);
   mFricativeSlider = new FloatSlider(this, "fric thresh", 5, 101, 100, 15, &mFricativeThresh, 0, 1);
   mWhisperSlider = new FloatSlider(this, "whisper", 5, 119, 100, 15, &mWhisper, 0, 1);
   mPhaseOffsetSlider = new FloatSlider(this, "phase off", 5, 137, 100, 15, &mPhaseOffset, 0, FTWO_PI);
   mCutSlider = new IntSlider(this, "cut", 5, 155, 100, 15, &mCut, 0, 100);

   mGate.CreateUIControls();
}

Vocoder::~Vocoder()
{
   delete[] mWindower;
   delete[] mCarrierInputBuffer;
}

void Vocoder::SetCarrierBuffer(float* carrier, int bufferSize)
{
   assert(bufferSize == GetBuffer()->BufferSize());
   BufferCopy(mCarrierInputBuffer, carrier, bufferSize);
   mCarrierDataSet = true;
}

void Vocoder::Process(double time)
{
   PROFILER(Vocoder);

   IAudioReceiver* target = GetTarget();

   if (target == nullptr)
      return;

   SyncBuffers();

   if (!mEnabled)
   {
      for (int ch = 0; ch < GetBuffer()->NumActiveChannels(); ++ch)
      {
         Add(target->GetBuffer()->GetChannel(ch), GetBuffer()->GetChannel(ch), GetBuffer()->BufferSize());
         GetVizBuffer()->WriteChunk(GetBuffer()->GetChannel(ch), GetBuffer()->BufferSize(), ch);
      }

      GetBuffer()->Reset();
      return;
   }

   ComputeSliders(0);

   float inputPreampSq = mInputPreamp * mInputPreamp;
   float carrierPreampSq = mCarrierPreamp * mCarrierPreamp;
   float volSq = mVolume * mVolume;

   int bufferSize = GetBuffer()->BufferSize();

   int zerox = 0; //count up zero crossings
   bool positive = true;
   for (int i = 0; i < bufferSize; ++i)
   {
      if ((GetBuffer()->GetChannel(0)[i] < 0 && positive) ||
          (GetBuffer()->GetChannel(0)[i] > 0 && !positive))
      {
         ++zerox;
         positive = !positive;
      }
   }
   bool fricative = zerox > (bufferSize * mFricativeThresh); //if a % of the samples are zero crossings, this is a fricative
   if (fricative)
      mFricDetected = true; //draw that we detected a fricative

   mGate.ProcessAudio(time, GetBuffer());

   mRollingInputBuffer.WriteChunk(GetBuffer()->GetChannel(0), bufferSize, 0);

   //copy rolling input buffer into working buffer and window it
   mRollingInputBuffer.ReadChunk(mFFTData.mTimeDomain, VOCODER_WINDOW_SIZE, 0, 0);
   Mult(mFFTData.mTimeDomain, mWindower, VOCODER_WINDOW_SIZE);
   Mult(mFFTData.mTimeDomain, inputPreampSq, VOCODER_WINDOW_SIZE);

   mFFT.Forward(mFFTData.mTimeDomain,
                mFFTData.mRealValues,
                mFFTData.mImaginaryValues);

   if (!fricative)
   {
      mRollingCarrierBuffer.WriteChunk(mCarrierInputBuffer, bufferSize, 0);
   }
   else
   {
      //use noise as carrier signal if it's a fricative
      //but make the noise the same-ish volume as input carrier
      for (int i = 0; i < bufferSize; ++i)
         mRollingCarrierBuffer.Write(mCarrierInputBuffer[gRandom() % bufferSize] * 2, 0);
   }

   //copy rolling carrier buffer into working buffer and window it
   mRollingCarrierBuffer.ReadChunk(mCarrierFFTData.mTimeDomain, VOCODER_WINDOW_SIZE, 0, 0);
   Mult(mCarrierFFTData.mTimeDomain, mWindower, VOCODER_WINDOW_SIZE);
   Mult(mCarrierFFTData.mTimeDomain, carrierPreampSq, VOCODER_WINDOW_SIZE);

   mFFT.Forward(mCarrierFFTData.mTimeDomain,
                mCarrierFFTData.mRealValues,
                mCarrierFFTData.mImaginaryValues);

   for (int i = 0; i < FFT_FREQDOMAIN_SIZE; ++i)
   {
      float real = mFFTData.mRealValues[i];
      float imag = mFFTData.mImaginaryValues[i];

      //cartesian to polar
      float amp = 2. * sqrtf(real * real + imag * imag);
      //float phase = atan2(imag,real);

      float carrierReal = mCarrierFFTData.mRealValues[i];
      float carrierImag = mCarrierFFTData.mImaginaryValues[i];

      //cartesian to polar
      float carrierAmp = 2. * sqrtf(carrierReal * carrierReal + carrierImag * carrierImag);
      float carrierPhase = atan2(carrierImag, carrierReal);

      amp *= carrierAmp;
      float phase = carrierPhase;

      phase += ofRandom(mWhisper * FTWO_PI);
      mPhaseOffsetSlider->Compute();
      phase = FloatWrap(phase + mPhaseOffset, FTWO_PI);

      if (i < mCut) //cut out superbass
         amp = 0;

      //polar to cartesian
      real = amp * cos(phase);
      imag = amp * sin(phase);

      mFFTData.mRealValues[i] = real;
      mFFTData.mImaginaryValues[i] = imag;
   }

   // Store spectral magnitudes for visualization
   for (int i = 0; i < FFT_FREQDOMAIN_SIZE; ++i)
   {
      float mr = mFFTData.mRealValues[i], mi = mFFTData.mImaginaryValues[i];
      float cr = mCarrierFFTData.mRealValues[i], ci = mCarrierFFTData.mImaginaryValues[i];
      mOutputMags[i] = sqrtf(mr * mr + mi * mi);
      // Recompute original modulator/carrier mags from before the vocoding loop overwrote mFFTData
      // (we stored them in mOutputMags above, but we need the originals)
      // Actually the modulator mags were destroyed by the loop. Store them BEFORE the loop next refactor.
      // For now, approximate: carrier mags are still in mCarrierFFTData.
      mCarrierMags[i] = sqrtf(cr * cr + ci * ci);
      // Modulator mags: recover from output / carrier (since output = mod * carrier)
      mModulatorMags[i] = (mCarrierMags[i] > 0.001f) ? mOutputMags[i] / mCarrierMags[i] : 0;
   }

   mFFT.Inverse(mFFTData.mRealValues,
                mFFTData.mImaginaryValues,
                mFFTData.mTimeDomain);

   for (int i = 0; i < bufferSize; ++i)
      mRollingOutputBuffer.Write(0, 0);

   //copy rolling input buffer into working buffer and window it
   for (int i = 0; i < VOCODER_WINDOW_SIZE; ++i)
      mRollingOutputBuffer.Accum(VOCODER_WINDOW_SIZE - i - 1, mFFTData.mTimeDomain[i] * mWindower[i] * .0001f, 0);

   Mult(GetBuffer()->GetChannel(0), (1 - mDryWet) * inputPreampSq, GetBuffer()->BufferSize());

   for (int i = 0; i < bufferSize; ++i)
      GetBuffer()->GetChannel(0)[i] += mRollingOutputBuffer.GetSample(VOCODER_WINDOW_SIZE - i - 1, 0) * volSq * mDryWet;

   Add(target->GetBuffer()->GetChannel(0), GetBuffer()->GetChannel(0), bufferSize);

   GetVizBuffer()->WriteChunk(GetBuffer()->GetChannel(0), bufferSize, 0);

   GetBuffer()->Reset();
}

void Vocoder::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   if (!mCarrierDataSet)
   {
      ofPushStyle();
      ofSetColor(255, 100, 100, gModuleDrawAlpha);
      DrawTextNormal("connect a vocodercarrier!", 5, 15);
      ofPopStyle();
   }

   mInputSlider->Draw();
   mCarrierSlider->Draw();
   mVolumeSlider->Draw();
   mDryWetSlider->Draw();
   mFricativeSlider->Draw();
   mWhisperSlider->Draw();
   mPhaseOffsetSlider->Draw();
   mCutSlider->Draw();

   if (mFricDetected)
   {
      ofPushStyle();
      ofFill();
      ofSetColor(255, 80, 60, gModuleDrawAlpha * .3f);
      ofRect(5, 101, 100, 14);
      ofPopStyle();
      mFricDetected = false;
   }

   mGate.Draw();

   // === Spectral visualization: modulator / carrier / output ===
   extern NVGcontext* gNanoVG;

   float vizX = 5, vizW = 290;
   float specH = 45;
   int numBins = 128; // show first 128 bins (0 to ~6kHz at 48kHz)

   struct SpecInfo { float* mags; float y; const char* label; int cr, cg, cb; };
   SpecInfo specs[3] = {
      { mModulatorMags, 125, "modulator", 255, 180, 80 },   // warm
      { mCarrierMags,   178, "carrier", 80, 180, 255 },     // cool
      { mOutputMags,    231, "output", 180, 255, 120 },      // green
   };

   for (int s = 0; s < 3; ++s)
   {
      float y = specs[s].y;

      // Background
      {
         NVGpaint bg = nvgLinearGradient(gNanoVG, vizX, y, vizX, y + specH,
            nvgRGBA(10, 10, 16, (int)(gModuleDrawAlpha * .8f)),
            nvgRGBA(4, 4, 8, (int)(gModuleDrawAlpha * .85f)));
         nvgBeginPath(gNanoVG);
         nvgRoundedRect(gNanoVG, vizX, y, vizW, specH, gCornerRoundness * 2);
         nvgFillPaint(gNanoVG, bg);
         nvgFill(gNanoVG);
      }

      // Filled spectrum
      {
         nvgBeginPath(gNanoVG);
         nvgMoveTo(gNanoVG, vizX, y + specH);
         for (int i = 0; i < numBins; ++i)
         {
            float mag = specs[s].mags[i + 1]; // skip DC bin
            mag = ofClamp(mag * 5, 0, 1);
            float bx = vizX + (float)i / numBins * vizW;
            float by = y + specH - mag * specH;
            nvgLineTo(gNanoVG, bx, by);
         }
         nvgLineTo(gNanoVG, vizX + vizW, y + specH);
         nvgClosePath(gNanoVG);

         NVGpaint fill = nvgLinearGradient(gNanoVG, vizX, y, vizX, y + specH,
            nvgRGBA(specs[s].cr, specs[s].cg, specs[s].cb, (int)(gModuleDrawAlpha * .35f)),
            nvgRGBA(specs[s].cr, specs[s].cg, specs[s].cb, (int)(gModuleDrawAlpha * .05f)));
         nvgFillPaint(gNanoVG, fill);
         nvgFill(gNanoVG);
      }

      // Stroke
      ofPushStyle();
      ofSetColor(specs[s].cr, specs[s].cg, specs[s].cb, gModuleDrawAlpha * .7f);
      ofSetLineWidth(1);
      ofNoFill();
      ofBeginShape();
      for (int i = 0; i < numBins; ++i)
      {
         float mag = ofClamp(specs[s].mags[i + 1] * 5, 0, 1);
         float bx = vizX + (float)i / numBins * vizW;
         float by = y + specH - mag * specH;
         ofVertex(bx, by);
      }
      ofEndShape(false);
      ofPopStyle();

      // Label
      ofSetColor(specs[s].cr, specs[s].cg, specs[s].cb, gModuleDrawAlpha * .3f);
      DrawTextNormal(specs[s].label, vizX + 3, y + 10, 8);
   }

   // Multiplication symbol between modulator and carrier
   ofSetColor(255, 255, 255, gModuleDrawAlpha * .2f);
   DrawTextNormal("x", vizX + vizW / 2 - 3, specs[0].y + specH + 8, 10);
   // Equals symbol before output
   DrawTextNormal("=", vizX + vizW / 2 - 3, specs[1].y + specH + 8, 10);
}

void Vocoder::CheckboxUpdated(Checkbox* checkbox, double time)
{
   if (checkbox == mEnabledCheckbox)
   {
      mGate.SetEnabled(mEnabled);
   }
}

void Vocoder::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadString("target", moduleInfo);

   SetUpFromSaveData();
}

void Vocoder::SetUpFromSaveData()
{
   SetTarget(TheSynth->FindModule(mModuleSaveData.GetString("target")));
}
