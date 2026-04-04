# BespokeSynth Topology Suite — Roadmap

Each milestone is a green build with new functionality you can hear and see.

---

## M0: Foundation (DONE)
- [x] Build pipeline (Sonoma universal + Sequoia arm64)
- [x] JUCE macOS 15 compatibility patch
- [x] Visual improvements (shadows, gradients, module-specific viz)
- [x] Bug fixes (VinylTempo, Canvas leak, Python errors, perf)
- [x] KarplusStrong string visualization
- [x] LatticeSynth (waveguide lattice, π₁)
- [x] CohomologySynth (modal synthesis, H*)
- [x] TopologySynth (combined multistage, skeuomorphic panels)
- [x] Documentation (AUDIT, ANALYSIS, DEV-NOTES, UI-NOTES, PHYSICS-MODELS, JOURNAL, CONTEXT)

## M1: TopologyFilter — the lattice as an effect
**Goal:** First topology-based audio EFFECT (not synth). Audio goes in, gets shaped by the lattice resonance, comes out.

- [ ] New module: `topologyfilter` (IAudioProcessor, not just IAudioSource)
- [ ] Accepts audio input — excites lattice continuously from incoming signal
- [ ] Note input for pitch tracking — lattice tunes to follow melody
- [ ] Wet/dry blend
- [ ] Same topology controls as TopologySynth (ring/fixed/mobius, nodes, reflection, damping)
- [ ] Visualization: lattice with audio flowing through it (input left, output right)
- [ ] Register, build, test

**Effort:** ~3 hours. Reuses 90% of LatticeSynth DSP.

## M2: CoupledOscillators — springs and masses
**Goal:** A physically intuitive synth with the most beautiful visualization in the suite.

- [ ] New module: `coupledosc`
- [ ] N masses (2-8) connected by springs with configurable coupling
- [ ] Per-mass natural frequency (ratio to fundamental)
- [ ] Verlet integration (stable, simple, energy-conserving)
- [ ] Excitation: pluck one or more masses on note-on
- [ ] Visualization: masses as circles, springs as wobbly lines, displacement animated
- [ ] Energy shown as color flowing between masses
- [ ] Beating patterns visible as slow amplitude modulation

**Effort:** ~4 hours. New DSP (simple), rich visualization.

## M3: Harden the core
**Goal:** Make the existing modules production-quality.

- [ ] Fix DC blocker: use exp(-2πf/sr) not linear approximation
- [ ] Add polyphony to TopologySynth via PolyphonyMgr (follow KarplusStrong pattern)
- [ ] Proper SaveState/LoadState for all three new synths
- [ ] Add Python scripting bindings for latticesynth and topologysynth
- [ ] Oversampling option for corruption nonlinearities (anti-aliasing)
- [ ] Parameter modulation: expose topology params as modulation targets
- [ ] Test at extreme settings (max nodes, max drive, zero damping)

**Effort:** ~6 hours. No new modules, just quality.

## M4: MembraneSynth — 2D waveguide mesh
**Goal:** The first 2D physical model. Drum heads, plates, gongs.

- [ ] New module: `membranesynth`
- [ ] 2D triangulated mesh (start with regular grid, 4x4 to 8x8)
- [ ] Clamped or free boundary conditions on the perimeter
- [ ] Strike position (x,y) determines which modes are excited
- [ ] Inharmonic spectrum (Bessel function zeros — THIS is what makes it sound like a drum)
- [ ] Visualization: top-down 2D heatmap of displacement, Chladni patterns
- [ ] Strike shown as ripple propagation from contact point

**Effort:** ~8 hours. New 2D DSP, significant visualization work.

## M5: CohomologyVerb — topology-shaped reverb
**Goal:** A reverb where the reflection structure comes from a simplicial complex.

- [ ] New module: `cohomologyverb` (audio effect)
- [ ] Feedback delay network with Laplacian-derived mixing matrix
- [ ] Delay lengths from complex edge weights
- [ ] β₁ controls diffusion density, β₂ controls coloration
- [ ] Preset complexes: room (simple), hall (torus-like), cathedral (high β₁), cave (high β₂)
- [ ] Size, damping, pre-delay controls
- [ ] Visualization: the complex with reverb energy flowing as pulses

**Effort:** ~8 hours. Significant DSP (FDN is well-studied but needs careful tuning).

## M6: Demo presets and polish
**Goal:** Showcase patches that demonstrate the full suite.

- [ ] Demo: "topology_exploration.bsk" — TopologySynth with notesequencer, cycling through topologies
- [ ] Demo: "lattice_filter.bsk" — TopologyFilter on a drum loop
- [ ] Demo: "coupled_bells.bsk" — CoupledOscillators with arpeggiation
- [ ] Demo: "membrane_kit.bsk" — MembraneSynth as a drum kit (multiple instances, different strike positions)
- [ ] Demo: "topology_verb.bsk" — CohomologyVerb on a dry vocal/instrument
- [ ] Polish: consistent panel sizes, color scheme, documentation links
- [ ] Polish: tooltips for all new controls

**Effort:** ~4 hours. No new code, just quality and presentation.

---

## Timeline

| Milestone | Status | Depends on |
|-----------|--------|------------|
| M0: Foundation | DONE | — |
| M1: TopologyFilter | Next | M0 |
| M2: CoupledOscillators | Next | M0 |
| M3: Harden core | After M1+M2 | M0 |
| M4: MembraneSynth | After M3 | M3 |
| M5: CohomologyVerb | After M3 | M3 |
| M6: Demo presets | After M4+M5 | All |

M1 and M2 are independent and can be built in parallel.
