# BespokeSynth UI — How to Compose, Change, Style, and Encode

Working notes from studying the rendering system. Updated as we learn.

---

## 1. Configuration Surface (what you can change without recompiling)

### userprefs.json (`~/Documents/BespokeSynth/userprefs.json`)

| Key | Current | What it does |
|-----|---------|-------------|
| `background_r/g/b` | 0.09, 0.09, 0.09 | Canvas background color (dark grey) |
| `lissajous_r/g/b` | 0.408, 0.245, 0.418 | Background Lissajous figure color (purple) |
| `draw_background_lissajous` | true | Animated waveform on canvas background |
| `cable_alpha` | 1.0 | Patch cable opacity |
| `fade_cable_middle` | true | Cables fade in the middle |
| `cable_quality` | 1.0 | Cable rendering quality (affects curves) |
| `motion_trails` | 1.0 | Visual motion trail effect |
| `draw_module_highlights` | true | Highlight border on active modules |
| `target_framerate` | 60.0 | UI render target FPS |
| `zoom` / `ui_scale` | 1.3 | Default zoom level |
| `grid_snap_size` | 30.0 | Module snap grid in pixels |
| `show_minimap` | false | Overview minimap |

### scriptstyles.json (`~/Documents/BespokeSynth/scriptstyles.json`)
Two themes: "classic" (dark) and "light" (solarized-like). Controls Python script editor colors.

---

## 2. Color System (in source code)

### Module category colors — `IDrawableModule.cpp:51-56`
Uses HSB color space. All categories share the same saturation (145) and brightness (220):

| Category | Hue | Approx Color |
|----------|-----|-------------|
| Note (kModuleCategory_Note) | 27 | Warm orange |
| Audio (kModuleCategory_Audio) | 135 | Teal/cyan |
| Synth/Instrument (kModuleCategory_Synth) | 79 | Yellow-green |
| NoteSource (kModuleCategory_Instrument) | 240 | Blue-violet |
| Pulse (kModuleCategory_Pulse) | 43 | Gold |
| Modulator (kModuleCategory_Modulator) | 200/100/255 | Bright cyan |
| Processor (kModuleCategory_Processor) | 170/100/255 | Bright teal |
| Other (kModuleCategory_Other) | 0 sat, 220 bright | Grey |

**To change colors**: edit the `sHue*`, `sSaturation`, `sBrightness` statics in `IDrawableModule.cpp:51-56`.
Or override `GetColor()` per-module for custom coloring.

### Background color
Set in userprefs.json via `background_r/g/b` (0.0-1.0 range). Currently very dark grey (0.09).

### Cable colors
Cables inherit the color of their connection type:
- Audio cables: teal (sHueAudio)
- Note cables: orange (sHueNote)
- Pulse cables: gold (hue 43)

---

## 3. Rendering Pipeline

### Draw order (ModularSynth.cpp Draw method)
1. Clear background with `background_r/g/b`
2. Draw Lissajous waveform if enabled
3. Draw grid (if snap enabled)
4. Draw all modules (back to front, via ModuleContainer)
5. Draw patch cables
6. Draw title bar / HUD
7. Draw minimap if enabled
8. Draw tooltips / quick spawn menu

### Module rendering (IDrawableModule.cpp:199+)
Each module draws:
1. **Background rect** — filled with category color, slight transparency
2. **Title bar** — module name, darker shade of category color
3. **Enabled checkbox** — circle in category color
4. **Module content** — via virtual `DrawModule()`
5. **Patch cable sources** — colored circles on edges
6. **Highlight border** — when selected/hovered

### NanoVG context
Global: `gNanoVG` and `gFontBoundsNanoVG`
All drawing goes through NanoVG (CPU-rendered vector graphics).

---

## 4. How to Make Visual Changes

### Quick wins (no rebuild needed)
- **Darker/lighter background**: edit `background_r/g/b` in userprefs.json
- **Cable style**: adjust `cable_alpha`, `fade_cable_middle`, `cable_quality`
- **Disable Lissajous**: set `draw_background_lissajous: false`
- **Script editor theme**: edit scriptstyles.json

### Source-level changes (need rebuild)
- **Module colors**: `IDrawableModule.cpp:51-56` — change HSB values
- **Module corner radius**: search for `nvgRoundedRect` in IDrawableModule.cpp
- **Title bar height**: look for title bar rect in Render()
- **Font size**: search for `nvgFontSize` calls
- **Cable thickness**: `IDrawableModule.cpp:586-601` — lineWidth, plugWidth
- **Control styling**: Slider.cpp, Checkbox.cpp, DropdownList.cpp each have their own DrawModule

### Skeuomorphic opportunities
JUCE and NanoVG can render:
- **Gradients** (`nvgLinearGradient`, `nvgRadialGradient`) — for beveled/raised surfaces
- **Shadows** (`nvgBoxShadow`) — depth on module backgrounds
- **Rounded corners** — already used, can be tuned
- **Textures** — NanoVG supports image fills (`nvgImagePattern`)
- **Blur** — via framebuffer effects (expensive but possible)

---

## 5. Observations from Running the Arpeggiation Demo

### First launch
- Workspace: `~/Documents/BespokeSynth/workspace.json` — tracks recent files
- Loaded: `example__arpeggiation.bsk`
- Audio: 48kHz, 256 buffer (good latency)
- UI scale: 1.3x

### Module layout in arpeggiation demo
(To be filled in after studying the .bsk file)

### Visual notes
- Module backgrounds are semi-transparent colored rectangles
- Cables are smooth curves, color-coded by type
- The Lissajous background adds depth without clutter
- Title bars are compact (good information density)
- Controls (sliders, dropdowns) are functional but very flat

---

## 6. Design Direction: Minimal Skeuomorphism

Goals:
- Keep the functional density (BespokeSynth packs a lot into small modules)
- Add subtle depth cues (shadows, very gentle gradients)
- Improve visual hierarchy (make active/playing modules pop)
- Keep the dark theme but refine the palette
- Don't touch the interaction model — just the visuals

### Palette concept (to be refined)
```
Background:     #141418  (slightly blue-tinted dark)
Module base:    category color at 15% opacity over #1a1a22
Module active:  category color at 25% opacity, subtle glow
Title bar:      category color at 40% opacity
Cable:          category color, 80% opacity, 2px
Text:           #e0e0e0 (primary), #808080 (secondary)
Accent:         #00cc99 (signal flow indicator)
```

### Experiments applied

**1. Drop shadows — ENABLED** (`IDrawableModule.cpp:238`)
- Was `kUseDropshadow = false` — someone wrote the whole system and turned it off
- Turned on with reduced shadow size (20→12) and gentler strength (0.2→0.15)
- Shadow is color-tinted per module category and brightens with audio activity (`highlight`)
- Uses `nvgBoxGradient` for smooth falloff

**2. Title bar gradient — APPLIED** (`IDrawableModule.cpp:281-287`)
- Replaced flat `ofRect` fill with `nvgLinearGradient`
- Top of title bar: category color at 35% * 80α
- Bottom of title bar: category color at 15% * 50α
- Uses `nvgRoundedRect` with `gCornerRoundness * 3` for softer top corners

**3. Corner radius — RUNTIME ADJUSTABLE**
- `gCornerRoundness` is already a slider in GlobalControls (0-2, default 1.0)
- Module rect passes `3 * gCornerRoundness` as corner radius
- No code change needed — adjust in-app via the globalcontrols module

### Still to try
- Subtle inner glow on modules producing audio (the `highlight` value is already computed)
- Cable rendering refinement (slightly thicker, softer endpoints)
- Grid dots instead of lines (less obtrusive)
- Module title text with slight text shadow for readability
