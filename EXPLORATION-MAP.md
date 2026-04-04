# Exploration Map — What's Possible From Here

An expansive survey of what we could build, play with, jam around, explore, and experiment with in BespokeSynth. Organized by the question: what would make the user's hands move faster and their ears open wider?

---

## I. Oscillator Instrument Designs with Deep Tuning Capacity

### 1. Morphing Wavetable
**Concept:** A 2D grid of waveforms. X axis = harmonic content, Y axis = waveform shape. The playback position in this grid is continuously modulatable — you morph between waveforms by moving through the space.

**What makes it deep:** Each waveform in the grid is editable (draw with mouse). Interpolation between grid points is bicubic (smooth in both dimensions). The grid itself becomes a control surface — drag to explore timbral space in real-time.

**Visualization:** The wavetable grid shown as a terrain map. Current playback position as a glowing dot. The waveform at that point drawn below as a live oscilloscope.

### 2. Additive Harmonic Sculptor
**Concept:** 32 individually controllable harmonics. Each has amplitude, phase, and per-harmonic detune. But instead of 32 sliders, the harmonics are displayed as a single editable spectral envelope curve. Draw the shape you want to hear.

**What makes it deep:** The spectral envelope is the primary control — you paint the harmonic profile. But you can also apply "spectral effects" that transform the envelope: spectral tilt (rolloff), odd/even balance, formant peaks, spectral stretch/compress.

**Visualization:** Bar graph of harmonic amplitudes with smooth interpolating curve overlay. Each harmonic bar glows with its instantaneous phase position. The resulting waveform shown below.

### 3. Phase Distortion Oscillator (Casio CZ style)
**Concept:** A sine wave whose phase is warped by a transfer function. The transfer function determines the timbre — a linear transfer = pure sine, a step function = pulse wave, a cubic = warm overdrive character. The transfer function is editable.

**What makes it deep:** Draw the phase transfer function. The oscillator instantly produces the corresponding waveform. Mathematical guarantee: any periodic waveform can be produced this way (it's equivalent to waveshaping, but parameterized by phase rather than amplitude).

**Visualization:** Split view: left = transfer function (editable curve), right = resulting waveform. As you draw the transfer function, the waveform updates in real-time. The mathematical duality is visible.

### 4. FM Cluster
**Concept:** Not a traditional 4-op FM synth, but a cluster of N oscillators (2-6) where any oscillator can modulate any other. The modulation routing is a directed graph — draw edges between nodes to create FM connections.

**What makes it deep:** The modulation graph IS the control surface. Each edge has a modulation depth slider. Feedback loops are allowed (with stability limiting). The graph topology determines the timbre family. This is the FM equivalent of our topology synths.

**Visualization:** Node graph (like our LatticeSynth viz) where nodes are oscillators and edges are modulation connections. Edge brightness = modulation depth. Node glow = output amplitude. The graph layout itself is the instrument's identity.

---

## II. Mathematically Beautiful & Unusual Filter Designs

### 5. Lattice Filter (already have the DSP!)
Our TopologyFilter is already this — a waveguide lattice used as a resonant filter. But it could be pushed further: make the lattice topology itself modifiable in real-time, driven by an LFO or envelope. The filter's character morphs as the topology changes.

### 6. Continued Fraction Filter
**Concept:** A filter whose transfer function is expressed as a continued fraction:
```
H(z) = 1 / (1 + a₁z⁻¹ / (1 + a₂z⁻¹ / (1 + a₃z⁻¹ / ...)))
```
Each "depth" of the fraction adds a resonant peak. The number of terms controls how many resonances exist. This is mathematically beautiful because the continued fraction converges — you can add or remove resonances continuously.

**What makes it unusual:** Most filters are polynomial (IIR) or ratio-of-polynomials (biquad cascade). A continued fraction filter has a different kind of regularity — each additional stage refines the previous response rather than adding an independent peak.

**Visualization:** The continued fraction tree shown graphically — each level branching, with the filter response evolving as you go deeper. The coefficients are the branch angles.

### 7. Möbius Transform Filter
**Concept:** Apply a Möbius transformation (az+b)/(cz+d) to the unit circle in the z-plane. This warps the frequency axis nonlinearly — it's like an allpass filter that can remap where frequencies live. Cascading multiple Möbius transforms creates exotic frequency warping.

**What makes it unusual:** Möbius transforms preserve circles and the unit circle maps to another circle. So the filter is always stable (if input is on the unit circle, output is too). The four parameters (a,b,c,d) give complete control over the warping.

**Visualization:** Two unit circles: before and after the transform. Points on the input circle map to points on the output circle. The warping is visible as a conformal deformation.

### 8. Fibonacci Comb Filter
**Concept:** A comb filter whose delay taps are at Fibonacci-numbered sample positions (1, 1, 2, 3, 5, 8, 13, 21, 34, 55...). The resulting comb has peaks at Fibonacci-ratio frequencies — an inharmonic spectrum that corresponds to the golden ratio.

**What makes it unusual:** Fibonacci ratios converge to the golden ratio φ = (1+√5)/2 ≈ 1.618. This is the "most irrational" number — the comb peaks are maximally spread, avoiding integer harmonics. The result sounds neither harmonic nor random — it's a unique metallic, crystalline quality.

**Visualization:** The delay taps shown as a spiral (golden spiral). Each tap as a dot on the spiral, glowing with its current amplitude. The resulting frequency response shown as peaks at golden-ratio positions.

### 9. Hilbert Transform Crossover
**Concept:** Use the Hilbert transform to decompose a signal into its analytic signal (amplitude envelope + instantaneous frequency). Then filter each component independently — shape the dynamics with one filter, shape the pitch content with another, and recombine.

**What makes it unusual:** This separates "how loud" from "which frequencies" — two orthogonal dimensions of the sound that are normally entangled. You can make a sound brighter without making it louder, or compress the dynamics without changing the spectrum.

### 10. Eigenfilter
**Concept:** Define a desired frequency response (draw it). Compute the optimal FIR filter coefficients via eigendecomposition of the autocorrelation matrix. The filter EXACTLY matches the drawn response at the specified frequency points.

**What makes it deep:** You literally draw the filter shape you want. The math produces the filter that matches it. No knobs, no Q, no cutoff — just draw. This is the most direct possible interface between intention and result.

**Visualization:** The drawn response curve overlaid with the actual filter response. The difference is the approximation error — visible as a faint residual line.

---

## III. UX Directions — Making It Nice to Play With

### 11. Gesture Recording
Record knob/slider movements as automation curves. Play them back synchronized to the transport. Every parameter becomes recordable.

### 12. Preset Morphing
Save multiple parameter snapshots. Morph between them using a 2D pad (like Snapshots but continuous). The X-Y position blends all parameters simultaneously.

### 13. Module Skins
Each module type can have multiple visual skins — default (current), minimal (just controls, no viz), expanded (full viz + spectrum), performance (large controls for live use).

### 14. Undo per Module
Not just Looper — every module should track its last N parameter states. Ctrl+Z on a focused module reverts its most recent change.

### 15. Module Groups
Select multiple modules, group them into a "rack" that collapses into a single unit with exposed macro controls. Like a prefab but live and editable.

---

## IV. What to Build Next — Ranked by Joy-per-Hour

| Idea | Effort | Joy | Build? |
|------|--------|-----|--------|
| **FM Cluster** (graph-based FM) | 6h | Very high | YES — it's our topology viz language applied to FM |
| **Fibonacci Comb** | 3h | High | YES — unique sound, golden spiral viz |
| **Additive Sculptor** | 5h | High | YES — draw-to-hear is powerful |
| **Phase Distortion** | 4h | High | YES — editable transfer function |
| Eigenfilter | 8h | Very high | Later — needs FIR infrastructure |
| Möbius Transform | 4h | Medium | Later — abstract, hard to hear the difference |
| Continued Fraction | 6h | Medium | Later — mathematical beauty over musical utility |
| Preset Morphing | 8h | Very high | Needs deep module system integration |
| Gesture Recording | 12h | Very high | Needs transport system hooks |
