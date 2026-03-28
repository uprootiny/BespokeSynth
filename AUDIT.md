# BespokeSynth — Code Audit Report (2026-03-28)

Comprehensive audit covering: broken/incomplete code, macOS compatibility, Python scripting gaps, and performance.

---

## 1. Critical Bugs

### VinylTempoControl — `mHasSignal` never set
- **File:** `Source/VinylTempoControl.h:47`
- `GetStopped()` always returns true because `mHasSignal` is initialized false and never updated
- **Impact:** Module is non-functional

### Canvas — Memory leak in RemoveElement
- **File:** `Source/Canvas.cpp:145`
- `//delete element; TODO(Ryan) figure out how to delete without messing up stuff accessing data from other thread`
- Elements removed from vector but never freed — leaks in long sessions

### VinylTempoControl — Stack overflow risk
- **File:** `Source/VinylTempoControl.cpp:189-196`
- `signed short data[8196]` on stack = 16KB, exceeds 16KB stack limit on Windows
- **Fix:** Move to heap allocation

---

## 2. Incomplete/Broken Modules

### ComboGridController — assert(false) in constructor
- **File:** `Source/ComboGridController.cpp:32`
- Cannot be instantiated at all. REGISTER_HIDDEN but still broken.
- Multiple `assert(false); //TODO(Ryan) implement` throughout

### Transport — Swing remainder calculation wrong
- **File:** `Source/Transport.cpp:437,453,466`
- `*remainderMs = 0; //TODO(Ryan) this is incorrect`
- Swing timing math returns zero instead of actual remainder

### LoopStorer — ChannelBuffer integration incomplete
- **File:** `Source/LoopStorer.cpp:146-147`
- `//TODO(Ryan) make loopstorer actually use ChannelBuffers`

### SampleFinder — Multichannel not supported
- **File:** `Source/SampleFinder.cpp:92`
- Only processes channel 0

### PitchRemap — Can't handle mapping changes during playback
- **File:** `Source/PitchRemap.cpp:120`

### Hidden/Experimental modules (REGISTER_HIDDEN):
VSTPlugin, SampleFinder, LoopStorer, ComboGridController, Autotalent, TakeRecorder, PitchChorus, Producer, ClipLauncher, EnvelopeEditor, Razor, KompleteKontrol (Mac-only), LFOController ("old, probably irrelevant")

---

## 3. macOS Compatibility Issues

### CRITICAL: CGWindowListCreateImage (macOS 15)
- **File:** `libs/JUCE/.../juce_Windowing_mac.mm:523-545`
- Obsoleted in macOS 15 SDK (not just deprecated — unavailable)
- **Status:** CI patch applied, returns empty Image on macOS 15+
- **Proper fix:** Implement ScreenCaptureKit (macOS 12.3+)

### HIGH: All NSOpenGL classes deprecated
- **Files:** `libs/JUCE/.../juce_OpenGL_mac.h`
- NSOpenGLPixelFormat, NSOpenGLContext, NSOpenGLView all deprecated
- CGLLockContext/CGLUnlockContext deprecated
- **Long-term fix:** Migrate to Metal

### HIGH: ProcessSerialNumber/TransformProcessType deprecated
- **File:** `libs/JUCE/.../juce_Process_mac.mm:28-31`
- **Fix:** Use `NSApplication setActivationPolicy:` instead

### MEDIUM: Deployment target too old
- **File:** `CMakeLists.txt:28`
- `CMAKE_OSX_DEPLOYMENT_TARGET 10.13` — macOS High Sierra
- Consider raising to 11.0 (Big Sur) for modern SDK compatibility

---

## 4. Python Scripting Gaps

### HIGH: No transport control from scripts
- Scripts can READ tempo/measure but CANNOT:
  - Set tempo, time signature, swing, loop points
  - Jump to measures, nudge timing
- Blocks adaptive/generative composition

### HIGH: Silent failures on control access
- `me.get("nonexistent")` returns 0.0 silently — no error, no warning
- `me.set("wrong_path", value)` silently drops the operation
- Makes debugging scripts extremely difficult

### HIGH: Missing Arpeggiator bindings
- Major sequencing module with zero Python exposure
- No interval, step size, octave repeat control from scripts

### MEDIUM: Scheduled operation overflow
- Fixed arrays: 200 notes, 50 method calls, 50 UI values
- Overflow is SILENT — notes just don't play

### MEDIUM: No audio analysis from scripts
- Can't query spectrum, FFT bins, peak levels
- Blocks audio-reactive generation

### MEDIUM: No sample loading from scripts
- Can fill buffers from Python lists but can't load files from disk

### MEDIUM: No control metadata
- Scripts can't query min/max ranges, step sizes, control types
- Can't validate their own inputs

### Coverage: 20 of 244+ modules have Python bindings
The generic `me.set()/me.get()` works for any control path, but dedicated bindings exist for only: bespoke, scriptmodule, notesequencer, drumsequencer, basslinesequencer, grid, notecanvas, sampleplayer, midicontroller, linnstrument, osccontroller, oscoutput, envelope, drumplayer, vstplugin, snapshots, interface, beats, abletongriddevice, module

---

## 5. Fix Priority Matrix

| Priority | Issue | Effort | Impact |
|----------|-------|--------|--------|
| **P0** | JUCE macOS 15 build fix | Done (CI patch) | Builds on Sequoia |
| **P1** | VinylTempoControl mHasSignal bug | 10 min | Module works |
| **P1** | Canvas memory leak | 30 min | No leak in long sessions |
| **P1** | Python silent failures → throw errors | 1 hour | Debuggable scripts |
| **P2** | Transport Python bindings | 2 hours | Generative composition |
| **P2** | Transport swing remainder math | 1 hour | Correct timing |
| **P2** | Stack overflow in VinylTempoControl | 10 min | Windows stability |
| **P2** | Arpeggiator Python bindings | 2 hours | Scriptable arps |
| **P3** | Raise deployment target to 11.0 | 30 min | Modern SDK compat |
| **P3** | Scheduled operation bounds checking | 1 hour | No silent drops |
| **P3** | Audio analysis Python bindings | 4 hours | Audio-reactive scripts |
| **P4** | ScreenCaptureKit integration | 8 hours | Window snapshots on Sequoia |
| **P4** | Metal migration (replace OpenGL) | Weeks | Future-proof rendering |

---

## 6. Quick Wins (< 30 minutes each)

1. **VinylTempoControl.h** — set `mHasSignal = true` in the signal detection path
2. **VinylTempoControl.cpp** — move `data[8196]` to heap: `auto data = std::make_unique<short[]>(8196);`
3. **ScriptModule_PythonInterface.i** — change `return 0.0f` to `throw std::runtime_error("Control not found: " + path)`
4. **MainComponent.cpp** — remove `#include "Push2Control.h" //TODO(Ryan) remove`
5. **StutterControl.cpp** — remove unreachable `assert(false)` after switch default
