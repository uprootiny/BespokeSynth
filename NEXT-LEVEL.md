# Next Level — Mechanical Accuracy, JUCE Leverage, Skeuomorphism

---

## What "mechanically accurate" means and where we fall short

### BowedString: the stick-slip model is first-order

Our friction model uses the hyperbolic approximation:
```
mu(v) = mu_d + (mu_s - mu_d) * v_break / (|v| + v_break)
```

This produces Helmholtz motion (correct) but misses:
- **Thermal hysteresis**: real rosin friction depends on temperature, which depends on recent velocity history. The bow "warms up" over the first few bow strokes — the tone brightens as rosin softens.
- **Transverse string motion**: we model only the transverse wave, not the torsional wave. Real bowed strings have a torsional component that creates the "scratchiness" heard during bow changes.
- **Bridge impedance mismatch**: our bridge filter is a fixed biquad. A real bridge has velocity-dependent impedance — it couples differently at different dynamic levels.

**To fix:** Replace the memoryless friction function with a state-variable model: mu(v, T) where T tracks a thermal state that decays toward ambient. Add a second waveguide pair for torsional waves with 4x the propagation speed. Make the bridge biquad coefficients velocity-dependent.

### MembraneSynth: the grid is isotropic

Our 2D FDTD uses a rectangular grid with uniform tension. A real drum head has:
- **Anisotropic tension**: higher tension near the rim (where it's stretched over the shell), lower in the center
- **Nonlinear large-amplitude effects**: at high excitation, the membrane tension increases with displacement (hardening spring). This raises the pitch during loud hits — the characteristic "pitch bend" of hand drums.
- **Air loading**: the air above and below the membrane couples to its vibration. For enclosed drums (toms, kick), the air cavity creates Helmholtz resonance that colors the low end.

**To fix:** Per-cell tension values (spatially varying). Add a displacement-dependent tension term: c² → c²(1 + α|u|²). Add a simple Helmholtz resonator (one biquad bandpass) coupled to the total membrane displacement.

### CoupledOscillators: damping is frequency-independent

We apply the same damping (mDamping) to all masses regardless of frequency. In real metallic objects:
- **Internal friction** is proportional to frequency: higher modes decay faster. The decay rate follows Qf = constant (constant quality factor × frequency product).
- **Radiation damping** is proportional to f² for small objects (Rayleigh radiation): higher modes radiate more efficiently.

**To fix:** Per-mass damping scaled by frequency ratio: damping_i = base_damping^(1 + 0.3 * log2(ratio_i)). This makes low modes sustain longer and high modes decay faster — the natural spectral evolution of struck metal.

---

## JUCE Features We're Not Leveraging

### juce::dsp::Oscillator
JUCE has bandlimited oscillators with anti-aliasing. We use raw sinf() everywhere. Switching to juce::dsp::Oscillator for the FM operators would eliminate aliasing at high frequencies.

### juce::dsp::IIR::Filter
We implement biquads from scratch in BowedString and CohomologyVerb. JUCE's IIR filter is optimized, tested, and supports coefficient smoothing. Using it would be cleaner and possibly faster.

### juce::dsp::Reverb
JUCE has a built-in reverb (Freeverb-based). We built CohomologyVerb from scratch for the topology premise, which is valid. But we could use JUCE's reverb as a comparison/fallback.

### juce::dsp::FFT
BespokeSynth has its own FFT (Source/FFT.h). JUCE's FFT is SIMD-optimized. For the Vocoder and any future spectral modules, switching to juce::dsp::FFT could be significantly faster.

### juce::AudioParameterFloat / AudioProcessorValueTreeState
JUCE has a full parameter management system with smoothing, undo, and preset save/load. BespokeSynth doesn't use it (it has its own slider system), but for complex modules like TopologySynth, the JUCE parameter tree could provide better state management.

### juce::OpenGLContext
BespokeSynth uses NanoVG (CPU rendering). JUCE supports OpenGL contexts for GPU-accelerated rendering. For the membrane heatmap and any future 3D visualization (CohomologyVerb graph), GPU rendering would be dramatically faster.

### juce::MidiKeyboardComponent
A visual piano keyboard widget. Could be embedded in our synth modules as an alternative note input — click piano keys directly in the module instead of needing a separate note source.

---

## Where Skeuomorphism Benefits Most

The modules where physical metaphor is strongest are where skeuomorphism adds the most:

### BowedString (highest benefit)
The instrument already IS a physical object. The visualization should look like a section of violin:
- Wood grain texture (NanoVG image pattern) on the body mode display area
- The bow as a translucent rectangular strip, not just a line
- String endpoints as small pegs/tuners
- F-hole shapes (stylized) flanking the body mode bars

### MembraneSynth (high benefit)
A drum head is tangible. The visualization could feel like looking down at a snare:
- Subtle leather/skin texture on the membrane area
- Rim as a metallic ring around the edge
- Strike point shown as an indentation, not just a crosshair
- Tension screws around the perimeter (one per grid edge)

### CoupledOscillators (medium benefit)
Gamelan mallets and bell surfaces. The visualization could show:
- Masses as metallic spheres with specular highlights
- Springs as coiled metal (not zigzag lines)
- The base (connection surface) as a wooden bar or metal plate

### FMCluster (medium benefit)
The DX7 is the iconic FM synth. A subtle nod:
- Operator nodes as rectangular "chips" instead of circles
- The modulation graph as PCB traces between chips
- A green-on-black aesthetic for the algorithm display

### TopologySynth (low benefit — already has panels)
The three-panel layout already provides visual structure. Pushing further:
- Panel screws at corners
- LED-style indicators for signal level at each stage
- Patch cables between stages shown as physical cables, not arrows

---

## The One Module to Build Next

Given all this — mechanical accuracy, JUCE leverage, skeuomorphism, and the draw-hear-play principle — the single highest-impact next module would be:

**An improved BowedString** that:
1. Uses state-dependent friction (thermal hysteresis)
2. Has per-mode frequency-dependent damping in the body resonator
3. Uses juce::dsp::IIR::Filter for the bridge and body biquads
4. Has a skeuomorphic violin-section visualization (wood texture, f-holes, pegs)
5. Responds to mouse drag for bowing (already partially implemented)

This would be the first module in the suite that is genuinely "mechanically accurate" — not just physically plausible but close enough to fool an acoustician.
