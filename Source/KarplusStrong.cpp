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
//  KarplusStrong.cpp
//  modularSynth
//
//  Created by Ryan Challinor on 2/11/13.
//
//

#include "KarplusStrong.h"
#include "OpenFrameworksPort.h"
#include "SynthGlobals.h"
#include "IAudioReceiver.h"
#include "ModularSynth.h"
#include "Profiler.h"

KarplusStrong::KarplusStrong()
: IAudioProcessor(gBufferSize)
, mPolyMgr(this)
, mNoteInputBuffer(this)
, mWriteBuffer(gBufferSize)
{
   mPolyMgr.Init(kVoiceType_Karplus, &mVoiceParams);

   AddChild(&mBiquad);
   mBiquad.SetPosition(150, 15);
   mBiquad.SetEnabled(true);
   mBiquad.SetFilterType(kFilterType_Lowpass);
   mBiquad.SetFilterParams(3000, sqrt(2) / 2);
   mBiquad.SetName("biquad");

   for (int i = 0; i < ChannelBuffer::kMaxNumChannels; ++i)
   {
      mDCRemover[i].SetFilterParams(10, sqrt(2) / 2);
      mDCRemover[i].SetFilterType(kFilterType_Highpass);
      mDCRemover[i].UpdateFilterCoeff();
   }
}

void KarplusStrong::CreateUIControls()
{
   IDrawableModule::CreateUIControls();
   mVolSlider = new FloatSlider(this, "vol", 3, 2, 80, 15, &mVolume, 0, 2);
   mInvertCheckbox = new Checkbox(this, "invert", mVolSlider, kAnchor_Right, &mVoiceParams.mInvert);
   mFilterSlider = new FloatSlider(this, "filter", mVolSlider, kAnchor_Below, 140, 15, &mVoiceParams.mFilter, 0, 5);
   mFeedbackSlider = new FloatSlider(this, "feedback", mFilterSlider, kAnchor_Below, 140, 15, &mVoiceParams.mFeedback, .5f, .9999f, 4);
   mSourceDropdown = new DropdownList(this, "source type", mFeedbackSlider, kAnchor_Below, (int*)&mVoiceParams.mSourceType, 52);
   mExciterFreqSlider = new FloatSlider(this, "x freq", mSourceDropdown, kAnchor_Right, 85, 15, &mVoiceParams.mExciterFreq, 10, 1000);
   mExciterAttackSlider = new FloatSlider(this, "x att", mSourceDropdown, kAnchor_Below, 69, 15, &mVoiceParams.mExciterAttack, 0.01f, 40);
   mExciterDecaySlider = new FloatSlider(this, "x dec", mExciterAttackSlider, kAnchor_Right, 68, 15, &mVoiceParams.mExciterDecay, 0.01f, 40);
   mVelToVolumeSlider = new FloatSlider(this, "vel2vol", mExciterAttackSlider, kAnchor_Below, 140, 15, &mVoiceParams.mVelToVolume, 0, 1);
   mVelToEnvelopeSlider = new FloatSlider(this, "vel2env", mVelToVolumeSlider, kAnchor_Below, 140, 15, &mVoiceParams.mVelToEnvelope, -1, 1);
   mPitchToneSlider = new FloatSlider(this, "pitchtone", mVelToVolumeSlider, kAnchor_Right, 125, 15, &mVoiceParams.mPitchTone, -2, 2);
   mLiteCPUModeCheckbox = new Checkbox(this, "lite cpu", mPitchToneSlider, kAnchor_Below, &mVoiceParams.mLiteCPUMode);
   //mStretchCheckbox = new Checkbox(this,"stretch",mVolSlider,kAnchor_Right,&mVoiceParams.mStretch);

   mSourceDropdown->AddLabel("sin", kSourceTypeSin);
   mSourceDropdown->AddLabel("white", kSourceTypeNoise);
   mSourceDropdown->AddLabel("mix", kSourceTypeMix);
   mSourceDropdown->AddLabel("saw", kSourceTypeSaw);
   mSourceDropdown->AddLabel("input", kSourceTypeInput);
   mSourceDropdown->AddLabel("input*", kSourceTypeInputNoEnvelope);

   mFilterSlider->SetMode(FloatSlider::kSquare);
   mExciterFreqSlider->SetMode(FloatSlider::kSquare);
   mExciterAttackSlider->SetMode(FloatSlider::kSquare);
   mExciterDecaySlider->SetMode(FloatSlider::kSquare);

   mBiquad.CreateUIControls();
}

KarplusStrong::~KarplusStrong()
{
}

void KarplusStrong::Process(double time)
{
   PROFILER(KarplusStrong);

   IAudioReceiver* target = GetTarget();

   if (!mEnabled || target == nullptr)
      return;

   SyncBuffers(mWriteBuffer.NumActiveChannels());

   mNoteInputBuffer.Process(time);

   ComputeSliders(0);

   int bufferSize = target->GetBuffer()->BufferSize();
   assert(bufferSize == gBufferSize);

   mWriteBuffer.Clear();
   mPolyMgr.Process(time, &mWriteBuffer, bufferSize);

   for (int ch = 0; ch < mWriteBuffer.NumActiveChannels(); ++ch)
   {
      Mult(mWriteBuffer.GetChannel(ch), mVolume, bufferSize);
      if (!mVoiceParams.mInvert) //unnecessary if inversion is eliminating dc offset
         mDCRemover[ch].Filter(mWriteBuffer.GetChannel(ch), bufferSize);
   }

   mBiquad.ProcessAudio(time, &mWriteBuffer);

   for (int ch = 0; ch < mWriteBuffer.NumActiveChannels(); ++ch)
   {
      GetVizBuffer()->WriteChunk(mWriteBuffer.GetChannel(ch), mWriteBuffer.BufferSize(), ch);
      Add(target->GetBuffer()->GetChannel(ch), mWriteBuffer.GetChannel(ch), gBufferSize);
   }

   GetBuffer()->Reset();
}

void KarplusStrong::PlayNote(NoteMessage note)
{
   if (!mEnabled)
      return;

   if (!NoteInputBuffer::IsTimeWithinFrame(note.time) && GetTarget())
   {
      mNoteInputBuffer.QueueNote(note);
      return;
   }

   if (note.velocity > 0)
      mPolyMgr.Start(note.time, note.pitch, note.velocity / 127.0f, note.voiceIdx, note.modulation);
   else
      mPolyMgr.Stop(note.time, note.pitch, note.voiceIdx);
}

void KarplusStrong::SetEnabled(bool enabled)
{
   mEnabled = enabled;
}

void KarplusStrong::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   mFilterSlider->Draw();
   mFeedbackSlider->Draw();
   mVolSlider->Draw();
   mSourceDropdown->Draw();
   mInvertCheckbox->Draw();
   mPitchToneSlider->Draw();
   mVelToVolumeSlider->Draw();
   mVelToEnvelopeSlider->Draw();
   mLiteCPUModeCheckbox->Draw();

   mExciterFreqSlider->SetShowing(mVoiceParams.mSourceType == kSourceTypeSin || mVoiceParams.mSourceType == kSourceTypeSaw || mVoiceParams.mSourceType == kSourceTypeMix);
   mExciterAttackSlider->SetShowing(mVoiceParams.mSourceType != kSourceTypeInputNoEnvelope);
   mExciterDecaySlider->SetShowing(mVoiceParams.mSourceType != kSourceTypeInputNoEnvelope);

   //mStretchCheckbox->Draw();
   mExciterFreqSlider->Draw();
   mExciterAttackSlider->Draw();
   mExciterDecaySlider->Draw();

   mBiquad.Draw();

   // === Vibrating string visualization ===
   {
      const float stringX = 5;
      const float stringY = 130;
      const float stringW = 265;
      const float stringH = 50;

      // Dark inset background
      {
         NVGpaint bg = nvgLinearGradient(gNanoVG, stringX, stringY, stringX, stringY + stringH,
            nvgRGBA(8, 10, 15, (int)(gModuleDrawAlpha * .9f)),
            nvgRGBA(4, 5, 8, (int)(gModuleDrawAlpha * .9f)));
         nvgBeginPath(gNanoVG);
         nvgRoundedRect(gNanoVG, stringX, stringY, stringW, stringH, gCornerRoundness * 2);
         nvgFillPaint(gNanoVG, bg);
         nvgFill(gNanoVG);
      }

      // Bridge endpoints (fixed nodes)
      ofColor color = GetColor(kModuleCategory_Synth);
      ofSetColor(color.r, color.g, color.b, gModuleDrawAlpha * .6f);
      ofFill();
      ofCircle(stringX + 4, stringY + stringH / 2, 3);
      ofCircle(stringX + stringW - 4, stringY + stringH / 2, 3);

      // Draw vibrating string from delay buffer of the most active voice
      float bestActivity = 0;
      KarplusStrongVoice* activeVoice = nullptr;
      for (int i = 0; i < kNumVoices; ++i)
      {
         const auto& info = mPolyMgr.GetVoiceInfo(i);
         if (info.mActivity > bestActivity && info.mVoice != nullptr)
         {
            auto* ksVoice = dynamic_cast<KarplusStrongVoice*>(info.mVoice);
            if (ksVoice && ksVoice->IsActive())
            {
               bestActivity = info.mActivity;
               activeVoice = ksVoice;
            }
         }
      }

      if (activeVoice != nullptr)
      {
         RollingBuffer& buf = activeVoice->GetDelayBuffer();
         int bufSize = buf.Size();
         if (bufSize > 0)
         {
            float amplitude = ofClamp(bestActivity * 3, 0, 1);

            // Filled string displacement with gradient
            nvgBeginPath(gNanoVG);
            nvgMoveTo(gNanoVG, stringX + 4, stringY + stringH / 2);
            for (int i = 0; i < (int)stringW - 8; ++i)
            {
               float t = (float)i / (stringW - 8);
               int bufIdx = (int)(t * bufSize) % bufSize;
               float sample = buf.GetSample(bufIdx, 0);
               float yOffset = sample * (stringH / 2 - 4) * amplitude;
               nvgLineTo(gNanoVG, stringX + 4 + i, stringY + stringH / 2 + yOffset);
            }
            nvgLineTo(gNanoVG, stringX + stringW - 4, stringY + stringH / 2);
            nvgLineTo(gNanoVG, stringX + stringW - 4, stringY + stringH);
            nvgLineTo(gNanoVG, stringX + 4, stringY + stringH);
            nvgClosePath(gNanoVG);
            {
               NVGpaint fill = nvgLinearGradient(gNanoVG, stringX, stringY, stringX, stringY + stringH,
                  nvgRGBA(color.r, color.g, color.b, (int)(amplitude * gModuleDrawAlpha * .25f)),
                  nvgRGBA(color.r * .2f, color.g * .2f, color.b * .2f, (int)(amplitude * gModuleDrawAlpha * .08f)));
               nvgFillPaint(gNanoVG, fill);
               nvgFill(gNanoVG);
            }

            // String stroke
            ofPushStyle();
            ofSetColor(color.r, color.g, color.b, amplitude * gModuleDrawAlpha);
            ofSetLineWidth(1.5f);
            ofNoFill();
            ofBeginShape();
            for (int i = 0; i < (int)stringW - 8; ++i)
            {
               float t = (float)i / (stringW - 8);
               int bufIdx = (int)(t * bufSize) % bufSize;
               float sample = buf.GetSample(bufIdx, 0);
               float yOffset = sample * (stringH / 2 - 4) * amplitude;
               ofVertex(stringX + 4 + i, stringY + stringH / 2 + yOffset);
            }
            ofEndShape(false);
            ofPopStyle();

            // Center rest line
            ofSetColor(255, 255, 255, gModuleDrawAlpha * .08f);
            ofSetLineWidth(.5f);
            ofLine(stringX + 6, stringY + stringH / 2, stringX + stringW - 6, stringY + stringH / 2);
         }
      }
      else
      {
         // Resting string — thin horizontal line
         ofSetColor(color.r * .4f, color.g * .4f, color.b * .4f, gModuleDrawAlpha * .5f);
         ofSetLineWidth(1);
         ofLine(stringX + 6, stringY + stringH / 2, stringX + stringW - 6, stringY + stringH / 2);

         // Rest position center line
         ofSetColor(255, 255, 255, gModuleDrawAlpha * .05f);
         ofSetLineWidth(.5f);
         ofLine(stringX + 6, stringY + stringH / 2, stringX + stringW - 6, stringY + stringH / 2);
      }
   }
}

void KarplusStrong::DrawModuleUnclipped()
{
   if (mDrawDebug)
   {
      mPolyMgr.DrawDebug(250, 0);
   }
}

void KarplusStrong::DropdownUpdated(DropdownList* list, int oldVal, double time)
{
}

void KarplusStrong::FloatSliderUpdated(FloatSlider* slider, float oldVal, double time)
{
}

void KarplusStrong::CheckboxUpdated(Checkbox* checkbox, double time)
{
   if (checkbox == mEnabledCheckbox)
   {
      mPolyMgr.KillAll();
      for (int ch = 0; ch < ChannelBuffer::kMaxNumChannels; ++ch)
         mDCRemover[ch].Clear();
      mBiquad.Clear();
   }
}

void KarplusStrong::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadString("target", moduleInfo);
   mModuleSaveData.LoadInt("voicelimit", moduleInfo, -1, -1, kNumVoices);
   EnumMap oversamplingMap;
   oversamplingMap["1"] = 1;
   oversamplingMap["2"] = 2;
   oversamplingMap["4"] = 4;
   oversamplingMap["8"] = 8;
   mModuleSaveData.LoadEnum<int>("oversampling", moduleInfo, 1, nullptr, &oversamplingMap);
   mModuleSaveData.LoadBool("mono", moduleInfo, false);

   SetUpFromSaveData();
}

void KarplusStrong::SetUpFromSaveData()
{
   SetTarget(TheSynth->FindModule(mModuleSaveData.GetString("target")));

   int voiceLimit = mModuleSaveData.GetInt("voicelimit");
   if (voiceLimit > 0)
      mPolyMgr.SetVoiceLimit(voiceLimit);
   else
      mPolyMgr.SetVoiceLimit(kNumVoices);

   bool mono = mModuleSaveData.GetBool("mono");
   mWriteBuffer.SetNumActiveChannels(mono ? 1 : 2);

   int oversampling = mModuleSaveData.GetEnum<int>("oversampling");
   mPolyMgr.SetOversampling(oversampling);
}
