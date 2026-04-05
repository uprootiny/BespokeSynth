# BespokeSynth Topology Suite — Context Map (updated 2026-04-05)

For resuming this work in a new session.

---

## 12 Custom Modules (all GREEN on CI)

| Module | Spawn | Type | DSP Core | Click-to-play? |
|--------|-------|------|----------|---------------|
| LatticeSynth | `latticesynth` | Synth | 1D waveguide, unitary KL scattering, Thiran allpass | Yes — click nodes |
| CohomologySynth | `cohomologysynth` | Synth | Jacobi eigensolver, phase quadrature modal synth | No |
| TopologySynth | `topologysynth` | Synth | 5-stage: exciter→lattice→shaper→modal→amp | No |
| CoupledOscillators | `coupledosc` | Synth | Verlet spring-mass, freq-dependent damping | Yes — click masses |
| MembraneSynth | `membranesynth` | Synth | 2D FDTD wave equation, bilinear heatmap | Yes — click to strike |
| BowedString | `bowedstring` | Synth | Stick-slip friction, dual waveguide, body modes | Yes — click to bow |
| FMCluster | `fmcluster` | Synth | N-op FM with arbitrary modulation graph | No |
| TopologyFilter | `topologyfilter` | Effect | Lattice as resonant audio filter | — |
| TopologyDelay | `topologydelay` | Effect | Delay + lattice-diffused feedback | — |
| CohomologyVerb | `cohomologyverb` | Effect | Householder FDN, topology presets | — |
| FibonacciComb | `fibonaccicomb` | Effect | Golden ratio comb, spiral viz | — |
| *(KarplusStrong)* | `karplusstrong` | *(enhanced)* | *(string viz added)* | — |

## 3 Prefab Layouts
- `topology_showcase.json` — all modules, basic wiring
- `topology_advanced.json` — nontrivial chains (filter→verb, shimmer)
- `topology_tanpura.json` — Indian classical (drone + sargam + tabla)

## Visual Enhancements (to existing BespokeSynth modules)
- Drop shadows, gradient title bars, upholstery (all modules)
- Gradient FloatSlider tracks
- KarplusStrong vibrating string
- SingleOscillator filled waveform
- Looper: beat grid, playhead glow, FourTet slices, mute overlay, pitch/loop count
- StepSequencer/NoteStepSequencer: playhead column glow
- SeaOfGrain: speed-coded glowing grain particles
- Vocoder: three-band spectral display (modulator × carrier = output)

## Key Documents
| File | Content |
|------|---------|
| AUDIT.md | Codebase bugs, deprecations, perf issues |
| ANALYSIS.md | Architecture overview (244 modules) |
| DEV-NOTES.md | How the UI is expressed, how to change it |
| UI-NOTES.md | Color system, config surface |
| PHYSICS-MODELS.md | 7 physics models for DSP+visual |
| JOURNAL.md | Dev journal with math derivations |
| REFLECTION.md | Quality self-assessment |
| MODULE-IDEAS.md | 8 module concepts with priority |
| DESIGN-CRITIQUE.md | Visual design assessment |
| DRAW-HEAR-PLAY.md | Direct manipulation design system |
| EXPLORATION-MAP.md | 15 ideas ranked by joy-per-hour |
| CROSS-DOMAIN-RESONANCE.md | Coupling maps between resonant systems |
| MORPHISM-PHYSICS.md | Energy transfer physics between spaces |
| BOWED-INSTRUMENT.md | Violin mathematical architecture |
| ALGORITHMIC-NOTES.md | DSP tricks audit |
| NEXT-LEVEL.md | Mechanical accuracy, JUCE, skeuomorphism |
| ROADMAP.md | Milestones M0-M6 |
| CONTEXT.md | This file |
| docs/index.html | Tutorial landing page |
| docs/01-simplicial-complexes.html | Ch1: building spaces |
| docs/02-cohomology.html | Ch2: cochains and sound |

## Build
- CI: GitHub Actions matrix (macos-14 Sonoma + macos-15 Sequoia)
- JUCE macOS 15 patch: python inline in CI workflow
- Latest build: `gh run download --repo uprootiny/BespokeSynth $(gh run list --repo uprootiny/BespokeSynth --limit 1 --json databaseId --jq '.[0].databaseId') --name BespokeSynth-macos-15-app --dir ~/Nissan/builds/BespokeSynth-latest`
- Total new C++ across all modules: ~5,000+ lines
