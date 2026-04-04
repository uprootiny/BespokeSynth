# Cross-Domain Resonance — Design Document

## The Insight

In every real instrument, the exciter and resonator are coupled through a physical interface (bridge, reed, lip, mallet contact). The coupling is accidental — whatever the physics of the interface happens to produce. Nobody asks: what if the coupling were a *design parameter*?

We can. We have waveguide lattices, simplicial complexes, modal synthesis, 2D meshes, spring-mass systems, and stick-slip friction. Each is a resonant system. The unexplored space is the *morphism between them* — how energy transfers from one topological space to another.

## Architecture: ResonanceCoupler

Two subsystems + a coupling map:

```
Exciter (any topology) ──→ Coupling Map ──→ Resonator (any topology) ──→ Output
                              ↑                          │
                              └──────── Feedback ────────┘
```

### Exciter Types (what creates the initial energy)
- **Impulse lattice**: a small waveguide lattice excited by a note (our LatticeSynth)
- **Bowed string**: continuous stick-slip friction (our BowedString)
- **Membrane strike**: 2D FDTD impulse (our MembraneSynth)
- **Coupled masses**: spring-mass energy injection (our CoupledOscillators)
- **Noise burst**: simple, cheap, generic

### Resonator Types (what shapes the energy)
- **Body modes**: parallel bandpass bank (violin body, guitar body, drum shell)
- **Simplicial complex**: eigenmode synthesis from topology (our CohomologySynth)
- **Waveguide lattice**: a second lattice with different topology
- **Tube**: 1D waveguide with register holes (clarinet, flute bore)
- **Plate**: 2D FDTD with different boundary conditions from the exciter

### The Coupling Map

This is the novel part. The coupling map defines HOW energy transfers between exciter and resonator. Mathematically, it's a linear map between the state spaces:

```
C: state(Exciter) → state(Resonator)
```

Different coupling maps:
- **Identity** (1:1): each exciter node maps to the corresponding resonator node. This is the simplest coupling — what you'd get if the exciter and resonator were the same object.
- **Projection**: exciter has more nodes than resonator. Multiple exciter modes map to the same resonator mode. This creates spectral compression — the resonator "summarizes" the exciter.
- **Embedding**: exciter has fewer nodes than resonator. Exciter energy is distributed across resonator modes. This creates spectral expansion — the resonator "elaborates" the exciter.
- **Rotation**: exciter mode i maps to resonator mode (i+k) mod N. This shifts which frequencies couple. Musically: the fundamental of the exciter drives a harmonic of the resonator instead of its fundamental.
- **Scramble**: random permutation. Each exciter mode drives a random resonator mode. Creates inharmonic, metallic, bell-like coupling.
- **Topology-preserving**: only modes with the same Betti number coupling. β₀ modes (fundamentals) only drive β₀ modes. β₁ modes (loops) only drive β₁ modes. This preserves the topological character across the coupling.

### Parameters

| Parameter | What it controls |
|-----------|-----------------|
| **exciter type** | Which synthesis engine creates the initial energy |
| **resonator type** | Which synthesis engine shapes the sustained sound |
| **coupling type** | How energy maps between the two (identity/project/embed/rotate/scramble) |
| **coupling strength** | How much energy transfers (0 = dry exciter, 1 = fully resonated) |
| **feedback** | How much resonator output feeds back into exciter (0 = one-way, creates self-sustaining sound at higher values) |
| **coupling offset** | For rotation coupling: how many modes to shift |
| **exciter params** | Topology, damping, etc. of the exciter |
| **resonator params** | Topology, damping, etc. of the resonator |

### Why This Is Novel

Existing synths have:
- Fixed coupling (violin: bridge is what it is)
- No coupling (additive: modes are independent)
- Uniform coupling (reverb: all modes mix equally)

Nobody has:
- **Topology-aware coupling** where the mathematical structure of the exciter maps onto the mathematical structure of the resonator
- **Coupling as a design parameter** that's as tweakable as filter cutoff
- **Visual representation of the coupling** — you can see which exciter modes drive which resonator modes

---

## FM Cluster — Design Document

### The Idea

Traditional FM synths (DX7) have fixed operator routing: 4 or 6 operators arranged in "algorithms" (preset topologies). You pick an algorithm and adjust ratios and depths.

FM Cluster removes the fixed algorithms. You have N oscillators (2-6) arranged as nodes in a graph. ANY node can modulate ANY other node. You draw the modulation connections by drawing edges. The graph IS the algorithm.

### DSP

Each oscillator i has:
- Phase φᵢ (advances at frequency fᵢ each sample)
- Frequency ratio rᵢ (relative to MIDI pitch)
- Output: sin(φᵢ + Σⱼ dᵢⱼ * sin(φⱼ))
  where dᵢⱼ = modulation depth from oscillator j to oscillator i

The modulation matrix D = [dᵢⱼ] is the adjacency matrix of the graph, weighted by modulation depths.

**Feedback**: diagonal entries dᵢᵢ > 0 create self-modulation (feedback FM), which produces noise-like spectra at high values. This is how the DX7 gets its characteristic "breath" and "sizzle."

**Stability**: for |D| spectral radius < ~10, the system is bounded. Above that, it produces noise. A soft limiter after each oscillator prevents blowup.

### Data shapes

- **Phase state**: N floats, each in [0, 2π)
- **Modulation matrix**: N×N floats, each in [-10, 10]
- **Frequency ratios**: N floats, each in [0.5, 16]
- **Output levels**: N floats, each in [0, 1] (which oscillators are "carriers" vs "modulators")

Total state: ~N² + 3N floats. For N=6: 54 floats. Tiny.

### Per-sample cost

For each sample: N sine evaluations + N² multiply-accumulates. For N=6: 6 sines + 36 MACs. Very cheap — much cheaper than our waveguide or FDTD modules.

### Visualization

This is where it gets good. The FM cluster IS a graph:

- **Nodes** = oscillators, arranged in a circle or force-directed layout
- **Edges** = modulation connections, thickness = depth, color = positive/negative
- **Node size** = output level (carriers are bigger)
- **Node color** = frequency ratio (low = warm, high = cool)
- **Self-loops** = feedback, drawn as small arcs from node to itself
- **Activity** = node glow from instantaneous output amplitude

When you play a note, you SEE the FM happening — energy flowing through the graph as modulation depths create and shape harmonics.

### Why this fits our framework

It's the same graph-visualization-is-the-DSP principle as LatticeSynth, but in the FM domain instead of the waveguide domain. The topology of the modulation graph determines the timbre family, just as the topology of the waveguide lattice determines the resonance structure.

### Musical character by graph shape

| Graph | FM Character |
|-------|-------------|
| Chain (A→B→C→carrier) | Classic DX7 algorithms |
| Star (all→center) | One carrier, many modulators = complex timbre |
| Ring (A→B→C→A) | Feedback FM ring = chaotic, evolving |
| Complete (all→all) | Maximum complexity, noise-like |
| Tree (branching) | Hierarchical harmonic structure |
| Disconnected | Independent oscillators = additive synthesis |

---

## Fibonacci Comb Filter — Design Document

### The Idea

A comb filter with delay taps at Fibonacci-numbered sample positions. The frequency peaks are at Fibonacci ratios — converging to the golden ratio φ ≈ 1.618.

### DSP

```
y[n] = x[n] + Σᵢ gⁱ * x[n - F(i)]
```

where F(i) is the i-th Fibonacci number and g is the feedback gain per tap.

Fibonacci numbers: 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144...

At 48kHz, these correspond to frequencies: 48k, 48k, 24k, 16k, 9.6k, 6k, 3.7k, 2.3k, 1.4k, 873, 539, 333 Hz.

The ratios between consecutive Fibonacci numbers converge to φ: 1/1, 2/1, 3/2, 5/3, 8/5, 13/8... → φ.

### What it sounds like

The golden ratio is the "most irrational" number — its continued fraction is [1; 1, 1, 1, ...]. This means the comb peaks are maximally spread across the spectrum, avoiding integer harmonics entirely.

The result: a metallic, crystalline, bell-like quality that's neither harmonic (like a string) nor white (like noise). It's the sound of irrationality itself.

### Visualization

A golden spiral. Each Fibonacci tap is a point on the spiral. The spiral rotates at the fundamental frequency. Tap amplitudes shown as glow intensity at each spiral point. The spiral naturally encodes the self-similar structure of the Fibonacci sequence.

### Parameters

| Parameter | Range | What |
|-----------|-------|------|
| fundamental | 20-2000 Hz | Scales all Fibonacci delays |
| depth | 0-12 | How many Fibonacci taps (more = richer) |
| feedback | 0-0.95 | Per-tap feedback gain |
| damping | 0-0.99 | HF absorption per tap |
| mix | 0-1 | Wet/dry blend |

### Why nobody has built this

Fibonacci numbers in audio aren't new (some reverb algorithms use them for delay lengths). But nobody has made a COMB filter specifically using Fibonacci-positioned taps with the golden spiral visualization. The mathematical beauty of φ as "maximally irrational" directly produces a unique acoustic character.
