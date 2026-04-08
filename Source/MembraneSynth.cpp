/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
**/

#include "MembraneSynth.h"
#include "OpenFrameworksPort.h"
#include "SynthGlobals.h"
#include "IAudioReceiver.h"
#include "ModularSynth.h"
#include "Profiler.h"
#include "ofxJSONElement.h"
#include "UIControlMacros.h"
#include "nanovg/nanovg.h"
#include <cmath>
#include <cstring>

MembraneSynth::MembraneSynth()
: IAudioProcessor(gBufferSize)
, mWriteBuffer(gBufferSize)
{
   mEnvelope.Set(1, 5, 0.7f, 600);
   ClearMesh();
}

MembraneSynth::~MembraneSynth()
{
}

void MembraneSynth::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   UIBLOCK0();
   INTSLIDER(mGridSizeSlider, "grid", &mGridSize, kMembraneMinSize, kMembraneMaxSize);
   DROPDOWN(mShapeDropdown, "shape", (int*)&mShape, 60);
   DROPDOWN(mBoundaryDropdown, "edge", (int*)&mBoundary, 60);
   FLOATSLIDER(mTensionSlider, "tension", &mTension, 0.05f, 0.49f);
   FLOATSLIDER_DIGITS(mDampingSlider, "sustain", &mDamping, 0.998f, 0.99999f, 5);
   FLOATSLIDER(mStrikeXSlider, "hit x", &mStrikeX, 0.1f, 0.9f);
   FLOATSLIDER(mStrikeYSlider, "hit y", &mStrikeY, 0.1f, 0.9f);
   FLOATSLIDER(mPickupXSlider, "ear x", &mPickupX, 0.1f, 0.9f);
   FLOATSLIDER(mPickupYSlider, "ear y", &mPickupY, 0.1f, 0.9f);
   FLOATSLIDER(mVolumeSlider, "vol", &mVolume, 0, 1);
   ENDUIBLOCK0();

   mShapeDropdown->AddLabel("square", kShape_Square);
   mShapeDropdown->AddLabel("circle", kShape_Circle);

   mBoundaryDropdown->AddLabel("clamp", kMembrane_Clamped);
   mBoundaryDropdown->AddLabel("free", kMembrane_Free);

   mEnvDisplay = new ADSRDisplay(this, "env", 3, 172, 120, 20, &mEnvelope);
}

void MembraneSynth::IntSliderUpdated(IntSlider* slider, int oldVal, double time)
{
   if (slider == mGridSizeSlider)
      ClearMesh();
}

void MembraneSynth::ClearMesh()
{
   memset(mP, 0, sizeof(mP));
   memset(mPprev, 0, sizeof(mPprev));
   memset(mPviz, 0, sizeof(mPviz));
}

bool MembraneSynth::IsInside(int x, int y)
{
   if (mShape == kShape_Square) return true;
   // Circle: distance from center <= radius
   float cx = (mGridSize - 1) * 0.5f;
   float cy = (mGridSize - 1) * 0.5f;
   float dx = x - cx, dy = y - cy;
   float r = mGridSize * 0.5f;
   return (dx * dx + dy * dy) <= r * r;
}

void MembraneSynth::PlayNote(NoteMessage note)
{
   if (note.velocity > 0)
   {
      mFrequency = 440.0f * powf(2.0f, (note.pitch - 69.0f) / 12.0f);
      mEnvelope.Start(gTime, note.velocity / 127.0f);

      // Clear and strike
      ClearMesh();

      // Strike position in grid coords
      int sx = (int)(mStrikeX * (mGridSize - 1));
      int sy = (int)(mStrikeY * (mGridSize - 1));
      sx = ofClamp(sx, 1, mGridSize - 2);
      sy = ofClamp(sy, 1, mGridSize - 2);

      // Gaussian impulse centered on strike point
      float vel = note.velocity / 127.0f;
      float sigma = 1.2f; // spread of the strike
      for (int y = 0; y < mGridSize; ++y)
      {
         for (int x = 0; x < mGridSize; ++x)
         {
            if (!IsInside(x, y)) continue;
            float dx = x - sx, dy = y - sy;
            float dist2 = dx * dx + dy * dy;
            mP[y][x] = vel * expf(-dist2 / (2 * sigma * sigma));
         }
      }
   }
   else
   {
      mEnvelope.Stop(gTime);
   }
}

void MembraneSynth::Process(double time)
{
   PROFILER(MembraneSynth);

   IAudioReceiver* target = GetTarget();
   if (!mEnabled || target == nullptr)
      return;

   SyncBuffers(1);
   ComputeSliders(0);

   int bufferSize = target->GetBuffer()->BufferSize();
   mWriteBuffer.Clear();
   float* out = mWriteBuffer.GetChannel(0);

   // Tension scales with pitch: higher notes = tighter membrane = faster propagation
   // Base tension at C4 (261Hz), scale with pitch ratio
   float pitchRatio = mFrequency / 261.63f;
   float c2 = mTension * sqrtf(pitchRatio); // Courant number squared
   c2 = std::min(c2, 0.499f); // stability: c² < 0.5

   // Pickup position in grid coords
   int px = ofClamp((int)(mPickupX * (mGridSize - 1)), 0, mGridSize - 1);
   int py = ofClamp((int)(mPickupY * (mGridSize - 1)), 0, mGridSize - 1);

   for (int s = 0; s < bufferSize; ++s)
   {
      mEnvValue = mEnvelope.Value(time);

      // 2D FDTD wave equation update (leapfrog / Verlet):
      // p_next[x][y] = 2*p[x][y] - p_prev[x][y]
      //              + c² * (p[x-1][y] + p[x+1][y] + p[x][y-1] + p[x][y+1] - 4*p[x][y])
      float pNext[kMembraneMaxSize][kMembraneMaxSize]{};

      for (int y = 0; y < mGridSize; ++y)
      {
         for (int x = 0; x < mGridSize; ++x)
         {
            if (!IsInside(x, y)) continue;

            // Boundary handling
            bool atEdge = (x == 0 || x == mGridSize - 1 || y == 0 || y == mGridSize - 1);
            if (atEdge && mBoundary == kMembrane_Clamped)
            {
               pNext[y][x] = 0; // clamped: zero at boundary
               continue;
            }

            // Neighbors (with boundary handling)
            float pL = (x > 0 && IsInside(x - 1, y)) ? mP[y][x - 1] : (mBoundary == kMembrane_Free ? mP[y][x] : 0);
            float pR = (x < mGridSize - 1 && IsInside(x + 1, y)) ? mP[y][x + 1] : (mBoundary == kMembrane_Free ? mP[y][x] : 0);
            float pU = (y > 0 && IsInside(x, y - 1)) ? mP[y - 1][x] : (mBoundary == kMembrane_Free ? mP[y][x] : 0);
            float pD = (y < mGridSize - 1 && IsInside(x, y + 1)) ? mP[y + 1][x] : (mBoundary == kMembrane_Free ? mP[y][x] : 0);

            float laplacian = pL + pR + pU + pD - 4.0f * mP[y][x];
            pNext[y][x] = 2.0f * mP[y][x] - mPprev[y][x] + c2 * laplacian;
            pNext[y][x] *= mDamping;
         }
      }

      // Swap buffers
      memcpy(mPprev, mP, sizeof(mP));
      memcpy(mP, pNext, sizeof(pNext));

      // Output: read at pickup position, hard-limit to prevent pops
      float sample = ofClamp(mP[py][px], -1.0f, 1.0f);
      out[s] = sample * mVolume * mEnvValue;

      time += gInvSampleRateMs;
   }

   // Copy mesh state for visualization (once per buffer)
   memcpy(mPviz, mP, sizeof(mP));

   GetVizBuffer()->WriteChunk(out, bufferSize, 0);
   Add(target->GetBuffer()->GetChannel(0), out, bufferSize);
   GetBuffer()->Reset();
}

// ============================================================
// VISUALIZATION: top-down heatmap of membrane displacement
// ============================================================

void MembraneSynth::GetModuleDimensions(float& width, float& height)
{
   width = 260;
   height = 390;
}

void MembraneSynth::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   // Controls
   mGridSizeSlider->Draw();
   mShapeDropdown->Draw();
   mBoundaryDropdown->Draw();
   mTensionSlider->Draw();
   mDampingSlider->Draw();
   mStrikeXSlider->Draw();
   mStrikeYSlider->Draw();
   mPickupXSlider->Draw();
   mPickupYSlider->Draw();
   mVolumeSlider->Draw();
   mEnvDisplay->Draw();

   // === Membrane heatmap ===
   float vizX = 10, vizY = 198, vizSize = 240;

   // Background
   {
      NVGpaint bg = nvgRadialGradient(gNanoVG, vizX + vizSize / 2, vizY + vizSize / 2,
         5, vizSize * 0.55f,
         nvgRGBA(12, 12, 18, (int)(gModuleDrawAlpha * .85f)),
         nvgRGBA(4, 4, 8, (int)(gModuleDrawAlpha * .9f)));
      nvgBeginPath(gNanoVG);
      if (mShape == kShape_Circle)
         nvgCircle(gNanoVG, vizX + vizSize / 2, vizY + vizSize / 2, vizSize / 2);
      else
         nvgRoundedRect(gNanoVG, vizX, vizY, vizSize, vizSize, gCornerRoundness * 3);
      nvgFillPaint(gNanoVG, bg);
      nvgFill(gNanoVG);
   }

   // Smooth interpolated membrane heatmap
   // Instead of per-cell circles, scan pixel rows and bilinearly interpolate
   // between grid points for a continuous surface appearance.
   {
      float pixelStep = 8.0f / gDrawScale; // render every ~8 screen pixels (was 3, caused stutter)
      if (pixelStep < 3) pixelStep = 3;

      for (float py = 0; py < vizSize; py += pixelStep)
      {
         for (float px = 0; px < vizSize; px += pixelStep)
         {
            // Map pixel to grid coords
            float gx = px / vizSize * (mGridSize - 1);
            float gy = py / vizSize * (mGridSize - 1);

            int x0 = (int)gx, y0 = (int)gy;
            int x1 = std::min(x0 + 1, mGridSize - 1);
            int y1 = std::min(y0 + 1, mGridSize - 1);
            float fx = gx - x0, fy = gy - y0;

            // Check circle mask at this pixel position
            if (mShape == kShape_Circle)
            {
               float cx = (mGridSize - 1) * 0.5f;
               float cy = (mGridSize - 1) * 0.5f;
               float dx = gx - cx, dy = gy - cy;
               if (dx * dx + dy * dy > cx * cx) continue;
            }

            // Bilinear interpolation
            float val = mPviz[y0][x0] * (1 - fx) * (1 - fy)
                      + mPviz[y0][x1] * fx * (1 - fy)
                      + mPviz[y1][x0] * (1 - fx) * fy
                      + mPviz[y1][x1] * fx * fy;

            float mag = ofClamp(fabsf(val) * 12, 0, 1);
            if (mag < 0.01f) continue;

            int r, g, b;
            if (val > 0) { r = (int)(255 * mag); g = (int)(140 * mag); b = (int)(50 * mag); }
            else         { r = (int)(50 * mag);  g = (int)(140 * mag); b = (int)(255 * mag); }

            float sx = vizX + px;
            float sy = vizY + py;

            nvgBeginPath(gNanoVG);
            nvgRect(gNanoVG, sx, sy, pixelStep, pixelStep);
            nvgFillColor(gNanoVG, nvgRGBA(r, g, b, (int)(mag * gModuleDrawAlpha * .65f)));
            nvgFill(gNanoVG);
         }
      }
   }

   // Strike position marker
   {
      float sx = vizX + mStrikeX * vizSize;
      float sy = vizY + mStrikeY * vizSize;
      ofPushStyle();
      ofNoFill();
      ofSetColor(255, 200, 100, gModuleDrawAlpha * .5f);
      ofSetLineWidth(1);
      ofCircle(sx, sy, 6);
      ofLine(sx - 8, sy, sx + 8, sy);
      ofLine(sx, sy - 8, sx, sy + 8);
      ofPopStyle();
   }

   // Pickup position marker
   {
      float px = vizX + mPickupX * vizSize;
      float py = vizY + mPickupY * vizSize;
      ofPushStyle();
      ofNoFill();
      ofSetColor(100, 200, 255, gModuleDrawAlpha * .5f);
      ofSetLineWidth(1);
      ofCircle(px, py, 5);
      // Small ear icon (two arcs)
      ofCircle(px, py, 8);
      ofPopStyle();
   }

   // Shape outline
   ofPushStyle();
   ofNoFill();
   ofSetColor(255, 255, 255, gModuleDrawAlpha * .1f);
   ofSetLineWidth(1);
   if (mShape == kShape_Circle)
      ofCircle(vizX + vizSize / 2, vizY + vizSize / 2, vizSize / 2);
   else
      ofRect(vizX, vizY, vizSize, vizSize, gCornerRoundness * 2);
   ofPopStyle();

   // Labels
   ofSetColor(255, 255, 255, gModuleDrawAlpha * .15f);
   DrawTextNormal("membrane", vizX + 5, vizY + vizSize - 4, 8);

   // Boundary label
   const char* edgeStr = mBoundary == kMembrane_Clamped ? "clamped" : "free";
   DrawTextNormal(edgeStr, vizX + vizSize - 40, vizY + vizSize - 4, 8);
}

void MembraneSynth::OnClicked(float x, float y, bool right)
{
   IDrawableModule::OnClicked(x, y, right);

   // Check if click is in the heatmap area
   float vizX = 10, vizY = 198, vizSize = 240;
   if (x >= vizX && x <= vizX + vizSize && y >= vizY && y <= vizY + vizSize)
   {
      // Convert click position to membrane coordinates (0-1)
      float hitX = (x - vizX) / vizSize;
      float hitY = (y - vizY) / vizSize;

      // Update strike position
      mStrikeX = ofClamp(hitX, 0.05f, 0.95f);
      mStrikeY = ofClamp(hitY, 0.05f, 0.95f);

      // Strike the membrane directly (like a note-on with velocity 100)
      ClearMesh();
      int sx = ofClamp((int)(mStrikeX * (mGridSize - 1)), 1, mGridSize - 2);
      int sy = ofClamp((int)(mStrikeY * (mGridSize - 1)), 1, mGridSize - 2);
      float sigma = 1.2f;
      for (int my = 0; my < mGridSize; ++my)
      {
         for (int mx = 0; mx < mGridSize; ++mx)
         {
            if (!IsInside(mx, my)) continue;
            float dx = mx - sx, dy = my - sy;
            mP[my][mx] = 0.3f * expf(-(dx * dx + dy * dy) / (2 * sigma * sigma));
         }
      }
      mEnvelope.Start(gTime, 0.5f);
   }
}

void MembraneSynth::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadString("target", moduleInfo);
   mModuleSaveData.LoadInt("grid", moduleInfo, 10, kMembraneMinSize, kMembraneMaxSize, true);
   SetUpFromSaveData();
}

void MembraneSynth::SetUpFromSaveData()
{
   SetTarget(TheSynth->FindModule(mModuleSaveData.GetString("target")));
   mGridSize = mModuleSaveData.GetInt("grid");
   ClearMesh();
}
