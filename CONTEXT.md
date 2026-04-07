# BespokeSynth Topology Suite — Context Map (session wrap-up)

For resuming this work in a new session.

---

## 13 Custom Modules (all GREEN on CI)

### Synths (8)
| Module | Spawn | DSP | Click-to-play |
|--------|-------|-----|--------------|
| LatticeSynth | `latticesynth` | 1D waveguide, KL scattering, Thiran allpass | Yes |
| CohomologySynth | `cohomologysynth` | Jacobi eigensolver, phase quadrature | Yes |
| TopologySynth | `topologysynth` | 5-stage: exciter→lattice→shaper→modal→amp | Yes |
| CoupledOscillators | `coupledosc` | Verlet spring-mass, freq-dependent damping | Yes |
| MembraneSynth | `membranesynth` | 2D FDTD, bilinear heatmap | Yes |
| BowedString | `bowedstring` | Stick-slip friction, dual waveguide, body modes | Yes |
| FMCluster | `fmcluster` | N-op FM, arbitrary modulation graph | Yes |
| WavetableMorph | `wavetable` | 4-slot morph, unison, bandlimited | Yes |

### Effects (5)
| Module | Spawn | DSP |
|--------|-------|-----|
| TopologyFilter | `topologyfilter` | Lattice resonance on audio |
| TopologyDelay | `topologydelay` | Delay + lattice-diffused feedback |
| CohomologyVerb | `cohomologyverb` | Householder FDN, 5 topology presets |
| FibonacciComb | `fibonaccicomb` | Golden ratio comb, spiral viz |

### Enhanced existing
| Module | What we added |
|--------|-------------|
| KarplusStrong | Vibrating string visualization |
| SingleOscillator | Filled gradient waveform |
| Looper | Beat grid, FourTet slices, mute overlay, pitch/loop indicators |
| StepSequencer | Yellow playhead column glow |
| NoteStepSequencer | Blue playhead column glow |
| SeaOfGrain | Speed-coded glowing grain particles |
| Vocoder | Three-band spectral display (mod × carrier = output) |
| All modules | Drop shadows, gradient title bars, upholstery |
| FloatSlider | Gradient tracks |

## 5 Prefab Layouts (.bsk + .json)
- `topology_showcase` — all modules, basic wiring
- `topology_advanced` — nontrivial effect chains
- `topology_tanpura` — Indian classical drone + sargam + tabla
- `topology_couperin` — Baroque harpsichord texture engine
- `topology_jazz_improv` — jazz combo (rhodes, solo, walking bass, brushes, ride)

## Known Issues / Acoustics
- **The synths produce sound but the acoustics need work.** The physical models are mathematically correct but not yet tuned to sound natural. Specific issues:
  - BowedString friction model needs thermal hysteresis for realistic bow changes
  - MembraneSynth lacks air loading (Helmholtz cavity coupling)
  - CoupledOscillators needs nonlinear stiffness for realistic bell spectra
  - LatticeSynth ring mode can sound harsh at high reflection
  - CohomologySynth eigenvalues give correct frequency ratios but modes lack natural decay curves
- **Prefab .bsk files are minimal** — they define module layout and connections but not slider values. User needs to adjust parameters after loading.

## Build
```bash
gh run download --repo uprootiny/BespokeSynth \
  $(gh run list --repo uprootiny/BespokeSynth --limit 1 --json databaseId --jq '.[0].databaseId') \
  --name BespokeSynth-macos-15-app --dir ~/Nissan/builds/BespokeSynth-latest
cd ~/Nissan/builds/BespokeSynth-latest && unzip -oq *.zip && xattr -cr BespokeSynth.app
open BespokeSynth.app
```

## Documentation (20 files)
| File | What |
|------|------|
| AUDIT.md | Codebase bugs, deprecations, performance |
| ANALYSIS.md | Architecture (244+ modules, signal types, rendering) |
| DEV-NOTES.md | How the UI works, how to change it |
| UI-NOTES.md | Color system, NanoVG usage, config surface |
| PHYSICS-MODELS.md | 7 physics models for DSP+visual |
| JOURNAL.md | Dev journal with mathematical derivations |
| REFLECTION.md | Quality self-assessment with scores |
| MODULE-IDEAS.md | 8 unrealized module concepts |
| DESIGN-CRITIQUE.md | Visual design assessment |
| DRAW-HEAR-PLAY.md | Direct manipulation design system |
| EXPLORATION-MAP.md | 15 ideas ranked by joy-per-hour |
| CROSS-DOMAIN-RESONANCE.md | Coupling maps between resonant systems |
| MORPHISM-PHYSICS.md | Energy transfer between topological spaces |
| BOWED-INSTRUMENT.md | Violin mathematical architecture |
| ALGORITHMIC-NOTES.md | DSP tricks: what's right, approximate, missing |
| NEXT-LEVEL.md | Mechanical accuracy, JUCE leverage, skeuomorphism |
| RESOURCE-ANALYSIS.md | Memory/compute budget (3.1MB total, 5.7% CPU) |
| ROADMAP.md | Milestones M0-M6 |
| CONTEXT.md | This file |
| docs/*.html | Tutorial pages (simplicial complexes, cohomology) |

## Session Stats
- 59 commits
- 82 files changed
- ~12,000 lines of new code
- 13 new modules + visual upgrades to 7 existing modules
- 5 prefab layouts
- 20 documentation files
