# BespokeSynth — Developer Notes

How the codebase is structured, how the UI is expressed, and how to work in it.

---

## Codebase Structure

### Layout
```
Source/              — ALL source files, flat (no subdirectories)
  ModularSynth.cpp   — the god object: audio I/O, rendering, module lifecycle
  IDrawableModule.*   — base class for every module
  ModuleFactory.cpp   — registry of 244+ module types
  SynthGlobals.*      — global state, constants, sample rate, buffers
  Transport.*         — tempo, time signature, swing, time listeners
  PolyphonyMgr.*      — voice allocation, stealing, fading
  PatchCable.*        — individual cable connections
  PatchCableSource.*  — cable origin points (the circles you drag from)
  Canvas.*            — grid/piano-roll/sample canvas widget
  ScriptModule.*      — Python embedding via pybind11
  [ModuleName].*      — one .h/.cpp pair per module (e.g., Looper.h/Looper.cpp)

libs/                — vendored dependencies as git submodules
  JUCE/              — audio framework, GUI primitives, VST hosting
  pybind11/          — Python C++ bindings
  nanovg/            — CPU vector graphics (gradients, shadows, curves)
  exprtk/            — expression parser
  ableton-link/      — tempo sync protocol
  tuning-library/    — microtonal tuning
  readerwriterqueue/ — lock-free queue

patches/             — CI-time source patches (JUCE macOS 15 fix)
.github/workflows/   — CI: matrix build for Sonoma + Sequoia
```

### Key Pattern: One Module = One File Pair
Every module in BespokeSynth follows the same pattern:
- `ModuleName.h` declares the class
- `ModuleName.cpp` implements it
- Class inherits from `IDrawableModule` + one or more interfaces (IAudioSource, INoteReceiver, etc.)
- Static `Create()` method for the factory
- Registered in `ModuleFactory.cpp` via `REGISTER(ClassName, "displayname", kModuleCategory_*)`

### Module Interface Hierarchy
```
IClickable                    — input/rendering base
├── IDrawableModule           — visible module with UI
│   └── [every concrete module]
│
IPatchable                    — can be connected via cables
├── IAudioSource              — produces audio buffers
├── IAudioReceiver            — consumes audio buffers
│   └── IAudioProcessor       — both source and receiver
├── INoteReceiver             — accepts MIDI note events
├── INoteSource               — emits MIDI note events
├── IPulseReceiver            — accepts trigger pulses
└── IPulseSource              — emits trigger pulses
```

### Signal Types (connection types)
| Type | Data | Interface | Cable Color |
|------|------|-----------|-------------|
| Audio | Float buffers at sample rate | IAudioSource → IAudioReceiver | Cyan (hue 135) |
| Note | Pitch/velocity/modulation events | INoteSource → INoteReceiver | Orange (hue 27) |
| Pulse | Trigger signals | IPulseSource → IPulseReceiver | Gold (hue 43) |
| Modulator | Control voltage (0-1 float) | ModulatorChain | Blue (hue 200) |
| UIControl | Parameter changes | IUIControl | — |
| Grid | Matrix data | GridModule | — |

---

## How the UI Is Expressed

### Rendering Architecture
BespokeSynth uses **immediate-mode rendering** — no scene graph, no retained objects. Every frame, everything is redrawn from scratch.

Two graphics layers work together:
1. **OpenFrameworks port** (`OpenFrameworksPort.h/cpp`) — `ofSetColor`, `ofRect`, `ofLine`, `ofCircle`, `ofBeginShape`/`ofVertex`/`ofEndShape`
2. **NanoVG** (`gNanoVG` global context) — `nvgBeginPath`, `nvgRoundedRect`, `nvgFillPaint`, gradients, shadows

The OF functions internally delegate to NanoVG, but the codestyle uses both directly. Use OF for simple shapes and NanoVG for effects (gradients, shadows, blur).

### Drawing Pipeline (every frame)

```
ModularSynth::Draw()                    — top of the stack
├── Clear background (sBackgroundR/G/B)
├── Draw Lissajous (if enabled)
├── Scale/translate for zoom/pan
├── Draw grid snap lines (if dragging)
├── ModuleContainer::DrawContents()
│   └── for each module:
│       └── IDrawableModule::Render()
│           ├── PreDrawModule()          — virtual hook
│           ├── DrawFrame()              — background, title, border
│           │   ├── Compute highlight     — from audio RMS
│           │   ├── Drop shadow           — nvgBoxGradient (if enabled)
│           │   ├── Background rect       — ofRect with category color
│           │   ├── Title bar             — nvgLinearGradient
│           │   ├── Inner fade            — nvgBoxGradient
│           │   ├── DrawModule()          — VIRTUAL: module-specific content
│           │   └── Outline/border        — ofRect with color
│           └── Draw patch cable sources
├── Draw patch cables
├── Draw title bar / HUD
├── Draw minimap (if enabled)
└── Draw tooltips
```

### Module-Specific Drawing
Each module implements `DrawModule()` which draws its content inside the module rect. The origin (0,0) is the top-left of the content area (below the title bar).

Typical pattern:
```cpp
void MyModule::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   // Draw custom visuals
   // ...

   // Draw UI controls
   mSlider->Draw();
   mButton->Draw();
   mDropdown->Draw();
}
```

Controls position themselves relative to the module. You create them in `CreateUIControls()` with pixel coordinates.

### Color System
Module colors use **HSB** (hue, saturation, brightness), defined as static floats in `IDrawableModule.cpp:51-56`:

```cpp
float sHueNote = 27;           // orange
float sHueAudio = 135;         // cyan
float sHueInstrument = 79;     // green
float sHueNoteSource = 240;    // blue-violet
float sSaturation = 145;       // shared saturation
float sBrightness = 220;       // shared brightness
```

`GetColor(ModuleCategory)` returns an `ofColor` from these values. Colors are used at various alpha levels:
- Module background: 25% of color, alpha 180
- Title bar: via nvgLinearGradient, 35%→15% of color
- Receive pips: full color
- Patch cables: full color, alpha from UserPrefs

### UI Controls
All controls inherit from `IUIControl` and draw themselves:

| Control | File | Visual Style |
|---------|------|-------------|
| FloatSlider | Slider.cpp:111 | Gradient track, red value line, LFO range bars |
| IntSlider | Slider.cpp (second Render) | Same pattern as FloatSlider |
| Checkbox | Checkbox.cpp:86 | Circle or rect, filled when checked |
| ClickButton | ClickButton.cpp | Rect with text, flash on click |
| DropdownList | DropdownList.cpp:166 | Colored rect, text, triangle indicator |
| RadioButton | RadioButton.cpp | Horizontal/vertical button group |
| TextEntry | TextEntry.cpp | Text input field |
| UIGrid | UIGrid.cpp | Interactive grid of cells (used by sequencers) |
| ADSRDisplay | ADSRDisplay.cpp | Interactive ADSR envelope editor |

### NanoVG Primitives Available

| Function | Purpose | Used in |
|----------|---------|---------|
| `nvgRoundedRect` | Rounded rectangle path | Module backgrounds, sliders |
| `nvgLinearGradient` | Top-to-bottom or left-to-right gradient | Title bars, slider tracks |
| `nvgRadialGradient` | Circular gradient | Playhead glows |
| `nvgBoxGradient` | Rect gradient with blur falloff | Drop shadows, column glows |
| `nvgBoxShadow` | Drop shadow effect | Module frames |
| `nvgBeginPath`/`nvgFill` | Path fill | All NanoVG drawing |
| `nvgStroke` | Path stroke | Outlines |
| `nvgImagePattern` | Texture fill | Not used yet — available for skeuomorphism |
| `nvgScissor` | Clipping rectangle | Used internally |

### Key Globals

| Global | Type | Purpose |
|--------|------|---------|
| `gNanoVG` | NVGcontext* | Active NanoVG context |
| `gDrawScale` | float | Current zoom level |
| `gModuleDrawAlpha` | float | Current module transparency (0-255) |
| `gCornerRoundness` | float | Corner radius multiplier (0-2, runtime adjustable) |
| `gTime` | double | Global transport time |
| `gSampleRate` | int | Audio sample rate |
| `gBufferSize` | int | Audio buffer size |

---

## How to Make Changes

### Adding a visual effect to a module
1. Find the module's `DrawModule()` in `Source/ModuleName.cpp`
2. Add NanoVG calls using `gNanoVG` context
3. Use `gModuleDrawAlpha` for opacity to respect minimize/dim
4. Use `GetColor(mModuleCategory)` for the module's category color
5. Rebuild and test

### Adding a new user preference
1. Add the field to `UserPrefs.h` in the appropriate section
2. It automatically appears in the prefs editor
3. Access via `UserPrefs.your_field.Get()`

### Adding a new module
1. Create `NewModule.h` and `NewModule.cpp` in `Source/`
2. Inherit from `IDrawableModule` + relevant interfaces
3. Implement `Create()`, `CreateUIControls()`, `DrawModule()`, `Process()`
4. Register in `ModuleFactory.cpp`: `REGISTER(NewModule, "newmodule", kModuleCategory_*)`
5. Add to `Source/CMakeLists.txt`

### Changing the color palette
Edit `IDrawableModule.cpp:51-56` and rebuild. Or at runtime, spawn a `globalcontrols` module — it has live HSB sliders.

---

## Our Visual Changes (fork additions)

| Change | File | Lines | Effect |
|--------|------|-------|--------|
| Drop shadows | IDrawableModule.cpp | 238-248 | Color-tinted, 12px, glow with audio |
| Gradient title bars | IDrawableModule.cpp | 276-287 | nvgLinearGradient top-bright to bottom-dark |
| Gradient slider tracks | Slider.cpp | 131-143 | nvgLinearGradient, rounded corners |
| Oscillator waveform | SingleOscillator.cpp | 280-340 | Filled gradient waveform + dark inset bg |
| Looper playhead glow | Looper.cpp | 650-680 | nvgRadialGradient at playhead position |
| Looper bar dividers | Looper.cpp | 682-688 | Full-height, subtle alpha |
| Looper waveform bg | Looper.cpp | 650-658 | Dark gradient inset behind waveform |
| StepSeq playhead glow | StepSequencer.cpp | 449-470 | Yellow nvgBoxGradient on current column |
| NoteSeq playhead glow | NoteStepSequencer.cpp | 234-252 | Blue nvgBoxGradient on current step |
