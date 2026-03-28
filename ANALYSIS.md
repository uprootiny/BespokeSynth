# BespokeSynth — Technical Analysis & Roadmap

## Fork Status
- **1 commit ahead** of upstream (your macOS Sequoia CI workflow)
- **145 commits behind** upstream (includes JUCE update that fixes macOS 15)
- Current build: **FAILED** — `CGWindowListCreateImage` obsoleted in macOS 15

---

## Architecture Overview

### What BespokeSynth Is
A **modular software synthesizer** — a visual patching environment where you wire together 244+ modules on an infinite canvas. Think Max/MSP or VCV Rack, but with Python scripting, a custom NanoVG renderer, and tight JUCE integration.

### Core Design
```
ModularSynth (master singleton)
├── ModuleFactory         — registry of 244+ module types
├── ModuleContainer       — hierarchical module tree
├── Transport             — tempo/timing with Ableton Link
├── PolyphonyMgr          — 32-voice polyphonic engine
├── PatchCable system     — 8 connection types
├── ScriptModule          — embedded Python 3 via pybind11
└── JUCE                  — audio I/O, VST/AU hosting, file formats
```

### Signal Flow Types
| Type | What flows | Interface |
|------|-----------|-----------|
| Audio | Float buffers at sample rate | IAudioSource → IAudioReceiver |
| Note | Pitch/velocity/modulation events | INoteSource → INoteReceiver |
| Pulse | Trigger/clock signals | IPulseSource → IPulseReceiver |
| Modulator | Control voltage (CV) | ModulatorChain |
| UIControl | Parameter changes | IUIControl |
| Grid | Matrix data | GridModule |

### Codebase Stats
- **731 files** (~360 .cpp + ~360 .h)
- **Flat structure** — all modules in Source/, one .cpp/.h pair each
- **C++17**, CMake 3.16+, NanoVG for 2D rendering
- **Key constants**: 16 voices (kNumVoices), 262K sample work buffer

---

## The 20 Most Important Files

| File | Role |
|------|------|
| ModularSynth.h/cpp | Master container — audio routing, module lifecycle, state |
| IDrawableModule.h | Base class for ALL modules — rendering, UI, save/load |
| ModuleFactory.h/cpp | Module registry — all 244+ types registered here |
| IAudioSource.h | Audio producer interface — `Process(double time)` |
| IAudioReceiver.h | Audio consumer — input ChannelBuffer management |
| IAudioProcessor.h | Bidirectional audio (source + receiver) |
| INoteReceiver.h | Note input — NoteMessage struct (pitch, vel, voice, mod) |
| INoteSource.h | Note output — NoteOutput wrapper for visualization |
| Transport.h | Tempo engine — ITimeListener, NoteInterval quantization |
| PolyphonyMgr.h | Voice allocation — stealing, fading, per-voice modulation |
| PatchCableSource.h | Visual cable origins — drag-and-drop patching |
| PatchCable.h | Individual connections — 8 ConnectionTypes |
| ChannelBuffer.h | Multi-channel float audio buffer |
| ScriptModule.h | Python embedding — 12+ pybind11 module bindings |
| Canvas.h | Step sequencer / note canvas UI widget |
| SynthGlobals.h | Global state — sample rate, buffers, time |
| EffectChain.h | Audio effect chain management |
| FileStream.h | Binary state serialization |
| ModuleContainer.h | Hierarchical module tree |
| OpenFrameworksPort.h | Lightweight OF compatibility layer |

---

## Module Catalog (244+ modules, 8 categories)

### Synths (13) — Sound generators
oscillator, fmsynth, karplusstrong, sampler, sampleplayer, drumplayer, drumsynth, seaofgrain, signalgenerator, metronome, beats, samplecanvas, + VST/AU plugins

### Audio (36) — Processing/effects
looper, looperrecorder, gain, effectchain, ringmodulator, fftvocoder, vocoder, stutter, multitapdelay, panner, send, waveshaper, eq, audiometer, spectrum, lissajous, feedback, buffershuffler, dcoffset, signalclamp, inverter, input, output, audiosplitter, splitter, audiorouter, freqdelay, waveformviewer, samplecapturer, samplergrid, vocodercarrier, + hidden experimental

### Note/Pitch (50+) — MIDI manipulation
chorder, arpeggiator, portamento, noteoctaver, capo, pitchbender, vibrato, pitchremap, quantizer, acciaccatura, velocityscaler, velocitysetter, velocitytocv, notefilter, noterangefilter, notechance, notegate, notelatch, notehocket, noteecho, noteratchet, notehumanizer, notestrummer, chorddisplayer, midioutput, midicc, mpesmoother, mpetweaker, + many more

### Sequencers/Instruments (20+)
drumsequencer, notesequencer, notecanvas, slidersequencer, euclideansequencer, circlesequencer, playsequencer, radiosequencer, dotsequencer, m185sequencer, rhythmsequencer, notelooper, randomnote, fouronthefloor, polyrhythms, midicontroller, gridkeyboard, keyboarddisplay

### Modulators/CV (30+)
envelope (ADSR), lfo, add, subtract, mult, curve, smoother, gravity, expression, accum, audiotocv, leveltocv, pitchtocv, pitchtospeed, notetofreq, macroslider, valuesetter, ramper, curvelooper, controlsequencer, gridsliders, fubble, pulsetrain, controlrecorder

### Pulse/Trigger (15+)
pulser, pulsesequence, notetopulse, audiotopulse, pulserouter, pulsechance, pulsedelayer, pulsegate, pulsehocket, pulseflag, pulselimit, pulsebutton, pulsedisplayer, boundstopulse

### Utility (30+)
script (Python!), snapshots, globalcontrols, comment, label, grid, oscoutput, songbuilder, timelinecontrol, samplebrowser, prefab, multitrackrecorder, abletonlink, clockin, clockout, push2control, savestateloader, timerdisplay, valuestream

---

## Python Scripting API

The ScriptModule embeds Python 3 via pybind11 with these bindings:

```python
import bespoke
bespoke.get_measure_time()     # current transport position
bespoke.get_step(subdivision)  # quantized step number
bespoke.get_scale()            # current musical scale
bespoke.get_tempo()            # BPM
bespoke.get_modules()          # all module paths
bespoke.pitch_to_freq(pitch)   # MIDI→Hz

import scriptmodule as me
me.play_note(pitch, vel, length, pan, output)
me.schedule_note(delay, pitch, vel, length)
me.schedule_call(delay, "method()")
me.set("control/path", value)
me.get("control/path")
```

Plus specialized bindings for: notesequencer, drumsequencer, grid, notecanvas, sampleplayer, midicontroller, envelope, drumplayer, vstplugin, module

---

## Build — What You Need (nix + Xcode)

### Prerequisites
```bash
xcode-select --install    # macOS frameworks (CoreAudio, CoreMIDI, etc.) — NOT optional
```

### Nix shell.nix
```nix
{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  buildInputs = with pkgs; [
    cmake ninja pkg-config python310
  ];
  shellHook = ''
    export BESPOKE_PYTHON_ROOT=${pkgs.python310}
  '';
}
```

### Build commands (arm64-only, fastest)
```bash
nix develop   # or nix-shell
git submodule update --init --recursive
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4 --config Release
./build/Source/BespokeSynth_artefacts/Release/BespokeSynth
```

### Build commands (universal binary for distribution)
```bash
cmake -Bbuild -GXcode \
  -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' \
  -DCMAKE_BUILD_TYPE=Release \
  -DBESPOKE_PORTABLE=True
cmake --build build --config Release --target BespokeSynth --parallel 4
```

### Current blocker
JUCE submodule pinned at commit `4f43011b` — uses `CGWindowListCreateImage` which is obsoleted in macOS 15 (Sequoia). Fix: merge upstream (they've updated JUCE).

---

## Roadmaps

### Roadmap 1: Get It Building (immediate)

1. Merge upstream main (145 commits) — fixes JUCE/Sequoia compatibility
2. Create nix flake for reproducible dev environment
3. Build arm64-only locally — verify it runs
4. Fix CI workflow to handle updated submodules
5. Add codesigning for local .app bundle

### Roadmap 2: Python Scripting Extensions

BespokeSynth's Python API is powerful but under-documented. Opportunities:
1. **Generative composition scripts** — algorithmic music using `schedule_note()` and `get_scale()`
2. **Live coding interface** — the ScriptModule already supports real-time code editing
3. **ML integration** — feed audio analysis into note generation (e.g., pitch detection → harmonizer)
4. **OSC bridge scripts** — connect to external tools via `ConnectOscInput(port)`
5. **Custom sequencer patterns** — Euclidean rhythms, Markov chains, L-systems via script

### Roadmap 3: Module Development

The module system is clean — one .cpp/.h pair, register in ModuleFactory, implement interfaces:
1. **New synth**: Wavetable oscillator (BespokeSynth lacks one)
2. **New effect**: Spectral freeze/blur (FFT-based, building on existing FFT infra)
3. **New sequencer**: Probability-weighted step sequencer (combine StepSequencer + NoteChance logic)
4. **New modulator**: Chaotic attractor CV source (Lorenz, Rössler systems)
5. **New utility**: OSC-to-note bridge module (currently only output exists)

### Roadmap 4: Distribution Pipeline

Your fork's CI workflow is the right approach, just needs fixes:
1. Fix the JUCE compatibility issue (merge upstream)
2. Universal binary (.app) via GitHub Actions on macos-15
3. DMG packaging (scripts/installer_macOS/ already has the tooling)
4. Notarization for Gatekeeper (scripts/notarize.sh exists)
5. Auto-release on tag push (your workflow already has this)

### Roadmap 5: Integration with Your Ecosystem

BespokeSynth + your other projects:
1. **Python scripts ↔ illuma-hyle** — shared Python tooling via uv
2. **OSC bridge ↔ tmux-web** — visualize BespokeSynth state in your web dashboards
3. **Ableton Link ↔ other audio tools** — networked tempo sync
4. **honeycomb integration** — add BespokeSynth as a node in your Shevat constellation monitoring
5. **Script library in claude-code-skills** — reusable BespokeSynth Python patterns
