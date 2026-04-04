# Physical Dynamics of Cross-Domain Resonance Coupling

How energy transfers between topologically different resonant systems, and what it sounds like.

---

## The Six Resonant Systems We Have

Each is a different kind of space with different physics. The state space, the wave equation, and the spectral character are all different.

### 1. Waveguide Lattice (LatticeSynth, TopologySynth)
**Space:** Graph G = (V, E) with delay lines on edges.
**State:** Pair (a⁺ᵢ, a⁻ᵢ) at each vertex — forward and backward traveling waves.
**Dynamics:** Scattering at vertices, propagation along edges. Energy conserved by unitary scattering matrix.
**Spectrum:** Determined by loop structure of G. Ring → harmonic. Tree → all modes decay. Möbius → octave-shifted.
**Dimension of state space:** 2|V| (two floats per vertex).

### 2. Simplicial Complex (CohomologySynth)
**Space:** Simplicial complex K with vertices, edges, faces.
**State:** p-cochains — functions assigning amplitudes to p-simplices.
**Dynamics:** Hodge Laplacian Δₚ = δδ* + δ*δ governs diffusion. Eigenvalues = frequencies².
**Spectrum:** Determined by Betti numbers. β₀ = fundamentals, β₁ = loop modes, β₂ = cavity modes.
**Dimension of state space:** |K₀| + |K₁| + |K₂| (one float per simplex).

### 3. Spring-Mass System (CoupledOscillators)
**Space:** N point masses connected by springs. Configuration space = ℝᴺ.
**State:** Position xᵢ and velocity vᵢ per mass.
**Dynamics:** Newton's second law: mẍᵢ = -kᵢxᵢ + Σⱼ cᵢⱼ(xⱼ - xᵢ). Verlet integration.
**Spectrum:** Eigenvalues of the coupling matrix. Beating between nearly-degenerate modes.
**Dimension of state space:** 2N (position + velocity per mass).

### 4. 2D Membrane (MembraneSynth)
**Space:** Rectangular or circular grid, discretized at N×M points.
**State:** Pressure p[x][y] at each grid point.
**Dynamics:** 2D wave equation via FDTD: p_next = 2p - p_prev + c²∇²p.
**Spectrum:** Bessel function zeros (circular) or product-of-sines (rectangular). Inharmonic.
**Dimension of state space:** 2NM (current + previous pressure fields).

### 5. Bowed String (BowedString)
**Space:** 1D interval [0, L], split at bow contact point.
**State:** Two traveling waves per segment (nut-side, bridge-side), bow contact state.
**Dynamics:** Stick-slip friction at bow point + waveguide propagation. Self-sustaining oscillation.
**Spectrum:** Nearly harmonic (string modes) shaped by body resonator (fixed filter bank).
**Dimension of state space:** 4 delay buffers + friction state.

### 6. FM Operator Network (FMCluster)
**Space:** Directed graph of N oscillators with N² modulation weights.
**State:** Phase φᵢ and output yᵢ per operator.
**Dynamics:** yᵢ = sin(φᵢ + Σⱼ dᵢⱼ yⱼ). Nonlinear — generates harmonics through modulation.
**Spectrum:** Bessel-function sidebands of the carrier. Depth controls harmonic richness.
**Dimension of state space:** 2N (phase + output per operator).

---

## The Morphism Problem

To couple system A to system B, we need a map:

```
C: state(A) → excitation(B)
```

This map must answer: when system A is in state s, how much energy enters system B, and WHERE in system B does it enter?

### The Three Components of a Coupling Map

**1. Spectral projection: which modes couple?**

System A has modes {aₖ} with frequencies {fₖ}. System B has modes {bₘ} with frequencies {gₘ}. The coupling efficiency between mode k and mode m depends on how close their frequencies are:

```
coupling(k, m) = exp(-(fₖ - gₘ)² / σ²)
```

where σ is the coupling bandwidth. Narrow σ = only exact matches couple (like sympathetic strings). Wide σ = everything couples (like a bridge that transmits all vibrations).

This is a Gaussian kernel on the frequency mismatch. It's the physically natural coupling law — resonant systems exchange energy most efficiently when their frequencies match.

**2. Spatial projection: where does energy enter?**

System A might have 8 nodes; system B might have 6 vertices. The spatial map determines which node in A drives which node in B.

Options:
- **Nearest-neighbor:** A's node i maps to B's nearest node (Euclidean in the visualization coordinates). Physical: like two objects touching at a point.
- **Distributed:** A's energy spreads uniformly across all of B's nodes. Physical: like acoustic radiation (sound fills the resonator).
- **Mode-shape matching:** A's mode shape vector is projected onto B's mode shape vectors via inner product. Physical: like resonance — the coupling preserves the spatial pattern of vibration.

**3. Impedance matching: how much energy transfers?**

In physics, impedance mismatch determines how much energy reflects vs. transmits at an interface. The impedance of a resonant system at frequency f is:

```
Z(f) = √(stiffness × mass) for mechanical systems
Z(f) = √(L/C) for electrical analogs
```

When Z_A(f) = Z_B(f), maximum energy transfer. When they differ, energy reflects back into A.

For our digital systems, the "impedance" is the magnitude of the Laplacian eigenvalue at each frequency. Systems with similar eigenvalue distributions couple efficiently; systems with very different distributions reflect most energy.

---

## Concrete Coupling Mechanics

### Lattice → Body Modes (violin-like)
**Physics:** String vibration reaches the bridge → bridge transfers to body.

The lattice's output is the sum of traveling waves at the bridge node (the rightmost node in a chain, or a designated pickup node in a ring). This single scalar drives the body mode bank.

```
bridge_signal = lattice.nodes[pickup].fwd + lattice.nodes[pickup].bwd
body_excitation[m] = bridge_signal * body.modes[m].coupling_strength
```

The coupling_strength per body mode is determined by the bridge transfer function (our biquad highpass at ~3kHz). Higher modes are coupled more strongly because the bridge is stiff — it transmits high-frequency vibration better than low.

**Feedback:** The body's output feeds back into the lattice at the bridge node, creating the sustained coupling that makes real instruments ring:
```
lattice.nodes[pickup].fwd += feedback * body_output * feedback_gain
```

### Membrane → Lattice (drum-into-resonator)
**Physics:** A drum head strikes, creating a 2D wave pattern → the drum shell channels this into 1D modes.

The membrane has NxN state values. The lattice has M nodes. The coupling map is a projection from 2D to 1D:

```
for each lattice node i:
  lattice_excitation[i] = Σ_{x,y} membrane.p[y][x] * coupling_kernel(x, y, i)
```

The coupling_kernel determines which region of the membrane drives which lattice node. A natural choice: divide the membrane perimeter into M segments, each corresponding to one lattice node. The rim of the drum IS the 1D lattice.

### Spring-Mass → Simplicial Complex (bell-into-cavity)
**Physics:** A bell's vibrating walls (coupled oscillators) drive the air inside (cavity modes).

The coupled oscillators have positions {xᵢ}. The simplicial complex has vertex amplitudes {vⱼ}. The coupling is through the Betti-2 modes (cavities):

```
cavity_pressure = Σᵢ coupling_weight[i] * oscillator[i].position
for each vertex j:
  complex.vertex[j] += cavity_pressure * cavity_mode_shape[j]
```

Only the β₂ modes (enclosed cavities) participate in this coupling. The β₁ modes (loops) couple differently — they correspond to air circulation, not pressure. And β₀ modes (connected components) only couple if the oscillators and complex are in the same connected component.

### FM Network → Waveguide (modulation-into-resonance)
**Physics:** No direct physical analog — this is a novel coupling.

The FM network produces a complex waveform. This waveform excites a waveguide lattice as if it were being "played" by the FM signal. The lattice resonance shapes the FM spectrum into something with physical resonance character.

```
fm_output = Σᵢ operator[i].output * operator[i].level
lattice.nodes[excite_node].fwd += fm_output * coupling_strength
```

This creates a hybrid timbre: FM's spectral richness + waveguide's physical resonance. The FM controls WHAT harmonics exist; the waveguide controls HOW they ring.

---

## Energy Conservation Across Coupling

For physical plausibility, energy must be accounted for. The total energy in the coupled system is:

```
E_total = E_A + E_B + E_coupling
```

where E_coupling is the energy stored in the coupling itself (the bridge, the air gap, the radiation field).

For stability:
```
E_coupling_extracted_from_A ≤ E_A * coupling_strength
E_coupling_injected_into_B = E_coupling_extracted_from_A * impedance_match_factor
E_reflected_back_to_A = E_coupling_extracted_from_A * (1 - impedance_match_factor)
```

The impedance_match_factor is in [0, 1]. At 0, no energy transfers (complete reflection). At 1, all energy transfers (perfect impedance match). In practice, real instruments are somewhere around 0.01-0.1 — most energy stays in the string/membrane and slowly leaks into the body.

This explains why instruments sustain: the exciter (string, membrane) holds energy and slowly radiates it through the coupling. If coupling were perfect, the note would be very loud and very short. Weak coupling = quiet but sustained.

---

## What This Sounds Like (Musical Predictions)

| Coupling | Sound |
|----------|-------|
| Lattice → Body (strong) | Loud, short, guitar-like pluck |
| Lattice → Body (weak) | Quiet, sustained, bowed-like sustain |
| Membrane → Lattice | Drum with tonal resonance (tabla, timpani) |
| Spring-Mass → Complex (β₂) | Bell with interior cavity resonance (church bell) |
| FM → Waveguide | Electric piano character (FM attack, physical sustain) |
| Waveguide → Membrane | String exciting a plate (prepared piano, zither on a table) |
| Complex → Complex (different topology) | Sound morphing (one topology evolving into another) |

---

## Implementation Path

The ResonanceCoupler module would:

1. Have two slots: Exciter (any synth engine) and Resonator (any synth engine)
2. A coupling matrix that maps exciter state → resonator excitation
3. A feedback path that maps resonator state → exciter modification
4. Coupling strength, bandwidth, and impedance match as controls
5. Visualization: the two topologies side by side with energy flow shown as animated particles crossing between them
