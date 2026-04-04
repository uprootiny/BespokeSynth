/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/
//
//  LatticeSynth.cpp
//  modularSynth
//
//  Waveguide lattice synth — see LatticeSynth.h for math documentation.
//

#include "LatticeSynth.h"
#include "OpenFrameworksPort.h"
#include "SynthGlobals.h"
#include "IAudioReceiver.h"
#include "ModularSynth.h"
#include "Profiler.h"
#include "ofxJSONElement.h"
#include "UIControlMacros.h"
#include "nanovg/nanovg.h"

LatticeSynth::LatticeSynth()
: IAudioProcessor(gBufferSize)
, mWriteBuffer(gBufferSize)
{
   mEnvelope.Set(3, 0, 1, 300);
   UpdateDelayLengths();
}

LatticeSynth::~LatticeSynth()
{
}

void LatticeSynth::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   UIBLOCK0();
   INTSLIDER(mNumNodesSlider, "nodes", &mNumNodes, 3, kMaxLatticeNodes);
   INTSLIDER(mExciteNodeSlider, "excite", &mExciteNode, 0, mNumNodes - 1);
   FLOATSLIDER_DIGITS(mDampingSlider, "damping", &mDamping, 0.9f, 1.0f, 4);
   FLOATSLIDER(mReflectionSlider, "reflect", &mReflection, 0.0f, 0.95f);
   FLOATSLIDER(mCorruptionDriveSlider, "drive", &mCorruptionDrive, 0.5f, 8.0f);
   FLOATSLIDER(mVolumeSlider, "vol", &mVolume, 0.0f, 1.0f);
   DROPDOWN(mBoundaryDropdown, "topology", (int*)&mBoundary, 80);
   DROPDOWN(mCorruptionDropdown, "corrupt", &mCorruptionType, 80);
   ENDUIBLOCK0();

   mBoundaryDropdown->AddLabel("fixed", kBoundary_Fixed);
   mBoundaryDropdown->AddLabel("free", kBoundary_Free);
   mBoundaryDropdown->AddLabel("ring", kBoundary_Ring);
   mBoundaryDropdown->AddLabel("mobius", kBoundary_Mobius);

   mCorruptionDropdown->AddLabel("none", kCorruption_None);
   mCorruptionDropdown->AddLabel("softclip", kCorruption_SoftClip);
   mCorruptionDropdown->AddLabel("fold", kCorruption_Fold);
   mCorruptionDropdown->AddLabel("rectify", kCorruption_Rectify);
}

void LatticeSynth::IntSliderUpdated(IntSlider* slider, int oldVal, double time)
{
   if (slider == mNumNodesSlider)
   {
      mExciteNodeSlider->SetExtents(0, mNumNodes - 1);
      if (mExciteNode >= mNumNodes)
         mExciteNode = mNumNodes / 3; // excite at ~1/3 for guitar-like timbre
      UpdateDelayLengths();
   }
}

// ============================================================
// DSP CORE
// ============================================================

void LatticeSynth::UpdateDelayLengths()
{
   // Total round-trip delay through all edges must equal sr/f0
   // For a ring: N edges, each of length L = sr / (f0 * N)
   // For a chain: N-1 edges, round-trip = 2*(N-1)*L = sr/f0
   //   so L = sr / (f0 * 2 * (N-1))

   float totalDelay = gSampleRate / mFrequency;

   int numEdges;
   if (mBoundary == kBoundary_Ring || mBoundary == kBoundary_Mobius)
      numEdges = mNumNodes;
   else
      numEdges = mNumNodes - 1;

   if (numEdges <= 0)
      numEdges = 1;

   float delayPerEdge;
   if (mBoundary == kBoundary_Ring || mBoundary == kBoundary_Mobius)
      delayPerEdge = totalDelay / numEdges;
   else
      delayPerEdge = totalDelay / (2.0f * numEdges); // round-trip

   int intDelay = std::max(1, std::min((int)delayPerEdge, kMaxDelayLength - 1));

   for (int i = 0; i < mNumNodes; ++i)
   {
      mNodes[i].delayLength = intDelay;
      mNodes[i].reflection = mReflection;
      mNodes[i].corruption = (LatticeCorruption)mCorruptionType;
      mNodes[i].corruptionDrive = mCorruptionDrive;
   }
}

float LatticeSynth::ApplyCorruption(float sample, LatticeCorruption type, float drive)
{
   switch (type)
   {
      case kCorruption_None:
         return sample;
      case kCorruption_SoftClip:
         return tanhf(sample * drive);
      case kCorruption_Fold:
      {
         // Wavefold: triangle-wave wrapping
         float x = sample * drive;
         x = x * 0.25f + 0.25f; // normalize to [0,1] range roughly
         x = x - floorf(x);     // frac
         return (fabsf(x * 4.0f - 2.0f) - 1.0f); // triangle fold
      }
      case kCorruption_Rectify:
         return fabsf(sample * drive) * (2.0f / std::max(drive, 0.5f));
      default:
         return sample;
   }
}

void LatticeSynth::ScatterAtNode(int i)
{
   // Kelly-Lochbaum scattering junction, parameterized by angle theta.
   // S = [cos(theta), sin(theta); sin(theta), -cos(theta)]
   // This is EXACTLY unitary (S^T S = I) for all theta.
   // theta=0: full transmission. theta=pi/2: full reflection.
   // The reflection parameter r in [0,1] maps to theta in [0, pi/2].
   float theta = mNodes[i].reflection * (FPI * 0.5f);
   float c = cosf(theta);
   float s = sinf(theta);

   float fwd = mNodes[i].forward;
   float bwd = mNodes[i].backward;

   float outFwd = c * fwd + s * bwd;
   float outBwd = s * fwd - c * bwd;

   // Apply corruption (nonlinearity)
   outFwd = ApplyCorruption(outFwd, mNodes[i].corruption, mNodes[i].corruptionDrive);
   outBwd = ApplyCorruption(outBwd, mNodes[i].corruption, mNodes[i].corruptionDrive);

   // DC blocker: proper first-order highpass at 20Hz
   // y[n] = x[n] - x[n-1] + R * y[n-1], where R = exp(-2*pi*fc/sr)
   // This gives -3dB at fc=20Hz and passes everything above.
   {
      static float sR = expf(-FTWO_PI * 20.0f / gSampleRate);
      float xn = outFwd;
      float yn = xn - mNodes[i].dcState + sR * mNodes[i].dcPrevY;
      mNodes[i].dcState = xn;   // x[n-1]
      mNodes[i].dcPrevY = yn;   // y[n-1]
      outFwd = yn;
   }

   mNodes[i].forward = outFwd * mDamping;
   mNodes[i].backward = outBwd * mDamping;
}

float LatticeSynth::ReadDelay(float* buffer, int writePos, int length, float samplesAgo, float& allpassState)
{
   // First-order Thiran allpass interpolation for fractional delay.
   //
   // Transfer function: H(z) = (a + z^-1) / (1 + a*z^-1)
   //   where a = (1-d)/(1+d) and d is the fractional part of the delay.
   //
   // Properties:
   //   |H(e^jw)| = 1 for all w  (flat magnitude — no coloration)
   //   Group delay ≈ d samples (correct fractional pitch)
   //   Requires one float of state per delay line (allpassState)
   //
   // This is strictly superior to linear interpolation, which has a
   // lowpass characteristic that dulls high frequencies on short delays.

   if (length <= 1) return buffer[0];

   int intDelay = (int)samplesAgo;
   float frac = samplesAgo - intDelay;

   int idx = (writePos - intDelay + length * 2) % length;

   if (frac < 0.001f)
   {
      allpassState = buffer[idx];
      return buffer[idx];
   }

   // Thiran coefficient
   float a = (1.0f - frac) / (1.0f + frac);

   // Allpass difference equation: y[n] = a * x[n] + x[n-1] - a * y[n-1]
   // Here x[n] = buffer[idx], x[n-1] = buffer[idx-1], y[n-1] = allpassState
   int idxPrev = (idx - 1 + length) % length;
   float y = a * buffer[idx] + buffer[idxPrev] - a * allpassState;
   allpassState = y;
   return y;
}

// PropagateForward/PropagateBackward removed — propagation is now inlined
// in Process() with snapshot ordering to avoid read-after-write hazard.

void LatticeSynth::ApplyBoundaryConditions()
{
   switch (mBoundary)
   {
      case kBoundary_Fixed:
         // Hard wall: reflect with sign inversion at both ends
         mNodes[0].backward = -mNodes[0].forward;
         mNodes[mNumNodes - 1].forward = -mNodes[mNumNodes - 1].backward;
         break;
      case kBoundary_Free:
         // Open end: reflect without inversion
         mNodes[0].backward = mNodes[0].forward;
         mNodes[mNumNodes - 1].forward = mNodes[mNumNodes - 1].backward;
         break;
      case kBoundary_Ring:
         // Periodic: forward from last wraps to first, backward from first wraps to last
         // Handled by making propagation indices wrap in Process()
         break;
      case kBoundary_Mobius:
         // Periodic + phase inversion: wrapping inverts the wave
         // Handled by making propagation indices wrap with negation in Process()
         break;
   }
}

void LatticeSynth::ExciteNode(int node, float amount)
{
   if (node >= 0 && node < mNumNodes)
   {
      mNodes[node].forward += amount;
      mNodes[node].backward -= amount * 0.3f; // asymmetric excitation
   }
}

void LatticeSynth::PlayNote(NoteMessage note)
{
   if (note.velocity > 0)
   {
      mPitch = note.pitch;
      mFrequency = 440.0f * powf(2.0f, (mPitch - 69.0f) / 12.0f);
      UpdateDelayLengths();
      mEnvelope.Start(gTime, note.velocity / 127.0f);
      mExciteAmount = note.velocity / 127.0f;

      // Clear all delay lines on new note to avoid pitch artifacts
      for (int i = 0; i < mNumNodes; ++i)
      {
         memset(mNodes[i].delayBuffer, 0, sizeof(mNodes[i].delayBuffer));
         memset(mNodes[i].delayBufferBack, 0, sizeof(mNodes[i].delayBufferBack));
         mNodes[i].forward = 0;
         mNodes[i].backward = 0;
         mNodes[i].dcState = 0;
         mNodes[i].dcPrevY = 0;
         mNodes[i].writePos = 0;
         mNodes[i].allpassStateFwd = 0;
         mNodes[i].allpassStateBack = 0;
      }
   }
   else
   {
      mEnvelope.Stop(gTime);
   }
}

void LatticeSynth::Process(double time)
{
   PROFILER(LatticeSynth);

   IAudioReceiver* target = GetTarget();
   if (!mEnabled || target == nullptr)
      return;

   SyncBuffers(1);
   ComputeSliders(0);

   // Update per-node params from globals (once per buffer, not per sample)
   for (int i = 0; i < mNumNodes; ++i)
   {
      mNodes[i].reflection = mReflection;
      mNodes[i].corruption = (LatticeCorruption)mCorruptionType;
      mNodes[i].corruptionDrive = mCorruptionDrive;
   }

   int bufferSize = target->GetBuffer()->BufferSize();
   mWriteBuffer.Clear();
   float* out = mWriteBuffer.GetChannel(0);

   for (int s = 0; s < bufferSize; ++s)
   {
      // Envelope
      mEnvelopeValue = mEnvelope.Value(time);

      // Excite on first few samples of a note
      if (mExciteAmount > 0)
      {
         float excite = mExciteAmount * mEnvelopeValue;
         // Noise burst excitation (pluck-like)
         excite *= RandomSample();
         ExciteNode(mExciteNode, excite);
         mExciteAmount *= 0.99f; // decay excitation over ~100 samples
         if (mExciteAmount < 0.001f)
            mExciteAmount = 0;
      }

      // Scatter at each node
      for (int i = 0; i < mNumNodes; ++i)
         ScatterAtNode(i);

      // Snapshot node states BEFORE propagation to avoid read-after-write hazard.
      // Without this, PropagateForward(0,1) modifies node[1].forward before
      // PropagateForward(1,2) reads it — corrupting the waveguide.
      float fwdSnapshot[kMaxLatticeNodes];
      float bwdSnapshot[kMaxLatticeNodes];
      for (int i = 0; i < mNumNodes; ++i)
      {
         fwdSnapshot[i] = mNodes[i].forward;
         bwdSnapshot[i] = mNodes[i].backward;
      }

      bool isRing = (mBoundary == kBoundary_Ring || mBoundary == kBoundary_Mobius);
      int propagateCount = isRing ? mNumNodes : mNumNodes - 1;

      // Propagate from snapshot: write delayed values into delay lines,
      // read delayed values into next node's forward/backward
      for (int i = 0; i < propagateCount; ++i)
      {
         int next = (i + 1) % mNumNodes;
         auto& src = mNodes[i];
         src.delayBuffer[src.writePos] = fwdSnapshot[i];
         mNodes[next].forward = ReadDelay(src.delayBuffer, src.writePos,
            src.delayLength, src.delayLength - 1, src.allpassStateFwd);
      }

      for (int i = propagateCount - 1; i >= 0; --i)
      {
         int next = (i + 1) % mNumNodes;
         auto& src = mNodes[next];
         src.delayBufferBack[src.writePos] = bwdSnapshot[next];
         mNodes[i].backward = ReadDelay(src.delayBufferBack, src.writePos,
            src.delayLength, src.delayLength - 1, src.allpassStateBack);
      }

      // Möbius: invert phase at the wrapping point
      if (mBoundary == kBoundary_Mobius)
      {
         mNodes[0].forward = -mNodes[0].forward;
         mNodes[mNumNodes - 1].backward = -mNodes[mNumNodes - 1].backward;
      }

      // Apply boundary conditions for non-ring topologies
      if (!isRing)
         ApplyBoundaryConditions();

      // Advance write positions
      for (int i = 0; i < mNumNodes; ++i)
         mNodes[i].writePos = (mNodes[i].writePos + 1) % std::max(1, mNodes[i].delayLength);

      // Output: sum of all forward waves at a pickup point (node 0)
      float output = 0;
      for (int i = 0; i < mNumNodes; ++i)
         output += mNodes[i].forward + mNodes[i].backward;
      output /= mNumNodes;
      output *= mVolume * mEnvelopeValue;

      out[s] = output;

      // Store visualization state (downsample to once per buffer)
      if (s == bufferSize / 2)
      {
         for (int i = 0; i < mNumNodes; ++i)
         {
            mNodeAmplitudes[i] = mNodes[i].forward + mNodes[i].backward;
            mNodeEnergies[i] = mNodes[i].forward * mNodes[i].forward + mNodes[i].backward * mNodes[i].backward;
         }
      }

      time += gInvSampleRateMs;
   }

   // Output: follow KarplusStrong pattern
   GetVizBuffer()->WriteChunk(out, bufferSize, 0);
   Add(target->GetBuffer()->GetChannel(0), out, bufferSize);
   GetBuffer()->Reset();
}

// ============================================================
// VISUALIZATION
// ============================================================

void LatticeSynth::GetModuleDimensions(float& width, float& height)
{
   width = 300;
   height = 260;
}

void LatticeSynth::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   // Draw controls
   mNumNodesSlider->Draw();
   mExciteNodeSlider->Draw();
   mDampingSlider->Draw();
   mReflectionSlider->Draw();
   mCorruptionDriveSlider->Draw();
   mVolumeSlider->Draw();
   mBoundaryDropdown->Draw();
   mCorruptionDropdown->Draw();

   // === Lattice Visualization ===
   const float vizX = 10;
   const float vizY = 62;
   const float vizW = 280;
   const float vizH = 190;
   const float centerX = vizX + vizW / 2;
   const float centerY = vizY + vizH / 2;

   // Dark background
   {
      NVGpaint bg = nvgLinearGradient(gNanoVG, vizX, vizY, vizX, vizY + vizH,
         nvgRGBA(8, 10, 18, (int)(gModuleDrawAlpha * .85f)),
         nvgRGBA(3, 4, 8, (int)(gModuleDrawAlpha * .85f)));
      nvgBeginPath(gNanoVG);
      nvgRoundedRect(gNanoVG, vizX, vizY, vizW, vizH, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, bg);
      nvgFill(gNanoVG);
   }

   ofColor color = GetColor(kModuleCategory_Synth);
   bool isRing = (mBoundary == kBoundary_Ring || mBoundary == kBoundary_Mobius);

   // Compute node positions
   float nodeX[kMaxLatticeNodes];
   float nodeY[kMaxLatticeNodes];

   if (isRing)
   {
      // Arrange in a circle
      float radius = std::min(vizW, vizH) * 0.35f;
      for (int i = 0; i < mNumNodes; ++i)
      {
         float angle = (float)i / mNumNodes * FTWO_PI - FPI / 2;
         nodeX[i] = centerX + cosf(angle) * radius;
         nodeY[i] = centerY + sinf(angle) * radius;
      }
   }
   else
   {
      // Arrange in a line
      float margin = 20;
      for (int i = 0; i < mNumNodes; ++i)
      {
         float t = (mNumNodes > 1) ? (float)i / (mNumNodes - 1) : 0.5f;
         nodeX[i] = vizX + margin + t * (vizW - margin * 2);
         nodeY[i] = centerY;
      }
   }

   // Draw edges (delay lines)
   int edgeCount = isRing ? mNumNodes : mNumNodes - 1;
   for (int i = 0; i < edgeCount; ++i)
   {
      int j = (i + 1) % mNumNodes;
      float energy = (mNodeEnergies[i] + mNodeEnergies[j]) * 0.5f;
      float brightness = ofClamp(sqrtf(energy) * 10, 0, 1);

      // Edge glow
      ofPushStyle();
      ofSetColor(color.r * (.2f + brightness * .6f),
                 color.g * (.2f + brightness * .6f),
                 color.b * (.2f + brightness * .6f),
                 gModuleDrawAlpha * (.3f + brightness * .5f));
      ofSetLineWidth(1.0f + brightness * 2.0f);
      ofLine(nodeX[i], nodeY[i], nodeX[j], nodeY[j]);
      ofPopStyle();

      // Traveling wave indicator (small dot moving along edge)
      float waveMag = fabsf(mNodeAmplitudes[i]);
      if (waveMag > 0.001f)
      {
         float progress = fmodf((float)gTime * 0.01f * mFrequency / 100.0f, 1.0f);
         float dotX = nodeX[i] + (nodeX[j] - nodeX[i]) * progress;
         float dotY = nodeY[i] + (nodeY[j] - nodeY[i]) * progress;

         NVGpaint glow = nvgRadialGradient(gNanoVG, dotX, dotY, 1, 6,
            nvgRGBA(255, 255, 255, (int)(waveMag * gModuleDrawAlpha * 200)),
            nvgRGBA(color.r, color.g, color.b, 0));
         nvgBeginPath(gNanoVG);
         nvgCircle(gNanoVG, dotX, dotY, 8);
         nvgFillPaint(gNanoVG, glow);
         nvgFill(gNanoVG);
      }
   }

   // Möbius twist indicator
   if (mBoundary == kBoundary_Mobius && mNumNodes > 0)
   {
      int last = mNumNodes - 1;
      float midX = (nodeX[last] + nodeX[0]) / 2;
      float midY = (nodeY[last] + nodeY[0]) / 2;
      ofPushStyle();
      ofSetColor(255, 100, 100, gModuleDrawAlpha * .6f);
      // Draw a small twist symbol (X)
      float s = 5;
      ofSetLineWidth(1.5f);
      ofLine(midX - s, midY - s, midX + s, midY + s);
      ofLine(midX - s, midY + s, midX + s, midY - s);
      ofPopStyle();
   }

   // Draw nodes
   for (int i = 0; i < mNumNodes; ++i)
   {
      float amplitude = mNodeAmplitudes[i];
      float energy = mNodeEnergies[i];
      float mag = ofClamp(sqrtf(energy) * 5, 0, 1);

      // Node displacement (for non-ring: vertical offset from center)
      float dispY = 0;
      if (!isRing)
      {
         dispY = amplitude * 40.0f; // scale displacement for visibility
      }

      float nx = nodeX[i];
      float ny = nodeY[i] + dispY;

      // Node glow (energy-based)
      if (mag > 0.01f)
      {
         NVGpaint glow = nvgRadialGradient(gNanoVG, nx, ny, 3, 15,
            nvgRGBA(color.r, color.g, color.b, (int)(mag * gModuleDrawAlpha * .5f)),
            nvgRGBA(color.r, color.g, color.b, 0));
         nvgBeginPath(gNanoVG);
         nvgCircle(gNanoVG, nx, ny, 18);
         nvgFillPaint(gNanoVG, glow);
         nvgFill(gNanoVG);
      }

      // Node circle
      float nodeRadius = 4 + mag * 4;
      ofPushStyle();
      ofFill();

      // Corruption indicator: node color shifts
      if (mNodes[i].corruption != kCorruption_None)
      {
         // Corrupted nodes glow warmer (orange shift)
         ofSetColor(color.r + 60 * mag, color.g * .7f, color.b * .5f,
                    gModuleDrawAlpha * (.6f + mag * .4f));
      }
      else
      {
         ofSetColor(color.r * (.4f + mag * .6f), color.g * (.4f + mag * .6f),
                    color.b * (.4f + mag * .6f), gModuleDrawAlpha * (.6f + mag * .4f));
      }
      ofCircle(nx, ny, nodeRadius);

      // Excite node indicator
      if (i == mExciteNode)
      {
         ofNoFill();
         ofSetColor(255, 255, 255, gModuleDrawAlpha * .5f);
         ofSetLineWidth(1);
         ofCircle(nx, ny, nodeRadius + 3);
      }

      ofPopStyle();
   }

   // For chain topology: draw the string shape connecting displaced nodes
   if (!isRing && mNumNodes > 1)
   {
      ofPushStyle();
      ofSetColor(color.r, color.g, color.b, gModuleDrawAlpha * .4f);
      ofSetLineWidth(1);
      ofNoFill();
      ofBeginShape();
      for (int i = 0; i < mNumNodes; ++i)
      {
         float dispY = mNodeAmplitudes[i] * 40.0f;
         ofVertex(nodeX[i], nodeY[i] + dispY);
      }
      ofEndShape(false);
      ofPopStyle();
   }

   // Topology label
   const char* topoLabel = "";
   switch (mBoundary)
   {
      case kBoundary_Fixed: topoLabel = "pi1 = {e}  fixed"; break;
      case kBoundary_Free: topoLabel = "pi1 = {e}  free"; break;
      case kBoundary_Ring: topoLabel = "pi1 = Z  ring"; break;
      case kBoundary_Mobius: topoLabel = "pi1 = Z  mobius"; break;
   }
   ofPushStyle();
   ofSetColor(255, 255, 255, gModuleDrawAlpha * .25f);
   DrawTextNormal(topoLabel, vizX + 5, vizY + vizH - 5, 9);
   ofPopStyle();
}

// ============================================================
// SAVE/LOAD
// ============================================================

void LatticeSynth::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadInt("num_nodes", moduleInfo, 8, 3, kMaxLatticeNodes, true);
   mModuleSaveData.LoadInt("boundary", moduleInfo, kBoundary_Ring);
   SetUpFromSaveData();
}

void LatticeSynth::SetUpFromSaveData()
{
   mNumNodes = mModuleSaveData.GetInt("num_nodes");
   mBoundary = (LatticeBoundary)mModuleSaveData.GetInt("boundary");
   UpdateDelayLengths();
}
