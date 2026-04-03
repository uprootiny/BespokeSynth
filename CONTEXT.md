# BespokeSynth Topology Suite — Context Map

For resuming this work in a new session.

---

## What exists (built and compiling)

### Three synthesis modules
| Module | Spawn name | DSP | Status |
|--------|-----------|-----|--------|
| **LatticeSynth** | `latticesynth` | 1D waveguide lattice, Kelly-Lochbaum unitary scattering, Thiran allpass delay, fixed/ring/Möbius boundaries, tanh/fold/rectify corruptions | GREEN, tested on CI |
| **CohomologySynth** | `cohomologysynth` | Simplicial cohomology modal synthesis, Jacobi eigendecomposition of graph Laplacian, 7 preset topologies, real Betti numbers via rank-nullity | GREEN, tested on CI |
| **TopologySynth** | `topologysynth` | Combined 5-stage: exciter → lattice → shaper → modal filter → amp. Skeuomorphic multi-panel UI. | Pushed, CI pending |

### Visual enhancements to existing modules
- Drop shadows on all modules (was disabled, turned on)
- Gradient title bars (nvgLinearGradient)
- Gradient FloatSlider tracks
- KarplusStrong: vibrating string visualization (draws actual delay buffer)
- SingleOscillator: filled gradient waveform with dark inset background
- Looper: playhead radial glow, dark waveform background, subtle bar dividers
- StepSequencer: yellow playhead column glow (nvgBoxGradient)
- NoteStepSequencer: blue playhead column glow

### Bug fixes
- VinylTempoControl: mHasSignal now updates, stack buffer moved to heap
- Canvas: memory leak fixed with deferred deletion queue
- Python scripting: me.get()/me.set() throw RuntimeError instead of silent 0.0
- Performance: oversampling division → multiplication, EffectChain skip at 100% wet

---

## Key files

| File | What's in it |
|------|-------------|
| `Source/LatticeSynth.h/cpp` | Waveguide lattice synth |
| `Source/CohomologySynth.h/cpp` | Simplicial cohomology modal synth |
| `Source/TopologySynth.h/cpp` | Combined multistage synth |
| `Source/IDrawableModule.cpp:238` | Drop shadow toggle (line 238, `kUseDropshadow`) |
| `Source/IDrawableModule.cpp:51-56` | Module category HSB colors |
| `Source/IDrawableModule.cpp:276` | Title bar gradient |
| `AUDIT.md` | Full codebase audit (bugs, deprecations, perf, Python gaps) |
| `ANALYSIS.md` | Architecture overview (244 modules, signal types, rendering) |
| `DEV-NOTES.md` | How the UI is expressed, how to make changes |
| `UI-NOTES.md` | Color system, config surface, design direction |
| `PHYSICS-MODELS.md` | 7 physics models brainstormed for DSP+visual |
| `JOURNAL.md` | Dev journal with mathematical derivations |
| `REFLECTION.md` | Honest quality assessment and score |
| `MODULE-IDEAS.md` | 8 new module concepts with priority matrix |
| `docs/index.html` | Tutorial landing page |
| `docs/01-simplicial-complexes.html` | Chapter 1: building spaces from triangles |
| `docs/02-cohomology.html` | Chapter 2: cochains, coboundary, Hodge Laplacian |

---

## The mathematical core

### What's implemented
- Coboundary operators δ⁰ (gradient) and δ¹ (curl) as dense matrices
- Graph Laplacian Δ₀ = (δ⁰)ᵀδ⁰ with Jacobi eigendecomposition
- Betti numbers via Gaussian elimination rank of δ matrices
- Kelly-Lochbaum unitary scattering junctions (cos θ / sin θ parameterization)
- Thiran first-order allpass fractional delay interpolation
- Modal synthesis from Laplacian eigenvalues (sinusoidal oscillator bank)

### What's NOT implemented
- Edge Laplacian Δ₁ and face Laplacian Δ₂ (would give β₁ and β₂ mode sounds)
- Cohomology ring product H^p ⊗ H^q → H^{p+q} as timbral mixing
- Smith Normal Form for torsion (relevant for non-orientable surfaces)
- 2D waveguide mesh (would model membranes/plates)
- Coupled oscillator system (mass-spring)

### Algorithmic notes
- Jacobi: uses proper two-sided similarity transform, handles degenerate eigenvalues
- Allpass: stateful per-edge, requires clearing on note-on
- DC blocker: coefficient should use exp(-2πf/sr) not linear approximation (currently linear)
- Scattering: guaranteed unitary for all r ∈ [0,1] via trigonometric parameterization

---

## Build system

- CI: GitHub Actions matrix (macos-14 Sonoma universal + macos-15 Sequoia arm64)
- JUCE macOS 15 fix: python inline patch in CI (patches CGWindowListCreateImage)
- Latest green build with all 3 synths: download from Actions artifacts
- Download command:
  ```
  gh run download --repo uprootiny/BespokeSynth \
    $(gh run list --repo uprootiny/BespokeSynth --limit 1 --json databaseId --jq '.[0].databaseId') \
    --name BespokeSynth-macos-15-app --dir ~/Nissan/builds/BespokeSynth-latest
  ```

---

## What to build next (priority order)

1. **TopologyFilter** — audio effect version of the lattice (reuses 90% of DSP)
2. **CoupledOscillators** — N spring-coupled masses, beautiful viz, gamelan sounds
3. **MembraneSynth** — 2D waveguide mesh, Chladni patterns, percussion
4. **CohomologyVerb** — FDN reverb with topology-shaped reflection structure
5. **TopologyLFO** — sub-audio lattice as modulation source

---

## User preferences (for the agent resuming this)

- Language: Swift/Rust/Clojure/Haskell preferred. C++ for BespokeSynth modules (required).
- No Python/Node except where required (Raycast, BespokeSynth pybind11)
- Nix for deps, uv for Python, no brew
- Commit often with clear messages
- Build via GitHub Actions, download artifacts
- BespokeSynth data dir: ~/Documents/BespokeSynth/
- BespokeSynth builds: ~/Nissan/builds/BespokeSynth*/
