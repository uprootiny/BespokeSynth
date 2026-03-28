# BespokeSynth Dev Journal

## 2026-03-28: LatticeSynth — from topology to timbre

### What we built

A waveguide lattice synthesizer that treats the fundamental group of its topology as a musical parameter.

The core insight: a chain of delay nodes with scattering junctions IS a lattice in the mathematical sense. The boundary conditions you choose determine the lattice's topology, which determines its fundamental group, which determines which resonances exist.

| Boundary | Space | π₁ | Musical effect |
|----------|-------|----| --------------|
| Fixed | Line segment [0,L] | {e} (trivial) | All harmonics, strong fundamental |
| Free | Line segment, open | {e} (trivial) | Odd harmonics suppressed at ends |
| Ring | Circle S¹ | Z | Pure harmonic series (all partials) |
| Möbius | Möbius strip | Z | Fundamental at half frequency (octave down) |

The corruptions (nonlinearities at nodes) break the lattice symmetry. In crystallographic terms, they're defects — vacancies or interstitials that create new scattering paths. In musical terms, they add harmonics and character:

- **SoftClip** (tanh): odd harmonics, warm saturation. The tanh function is the simplest symmetry-preserving nonlinearity.
- **Fold** (triangle wave wrap): rich harmonics, metallic. This is a period-doubling map that creates dense spectral content.
- **Rectify** (|x|): even harmonics, hollow. Breaks the odd-symmetry of the waveform.

### The visualization

The lattice is drawn as its actual topological space:
- **Ring/Möbius**: nodes on a circle, edges connecting them
- **Fixed/Free**: nodes on a line, edges as segments

What's shown is NOT a simulation — it's the DSP state itself:
- Node displacement = forward + backward traveling wave amplitude
- Edge brightness = energy flowing through the delay line
- Traveling dots = wave propagation direction
- Node glow = energy at that scattering junction
- Corruption color shift = orange warmth at nonlinear nodes

### Why this matters

Most synthesizers hide their math. BespokeSynth's modular canvas is already a graph — adding a module whose internal structure is ALSO a visible graph creates a natural visual language. You can see the topology. You can see the waves. You can see where the corruption lives.

The Möbius twist is the star: a small red X marks where the phase inverts at the wrapping point. This single topological feature drops the fundamental by an octave — the smallest possible change to the space creates the largest possible change to the pitch.

### What's precise about the DSP

**Data shape at each node:**
```
struct: { forward: float, backward: float }
```
This is a section of a rank-2 trivial bundle over the graph. The scattering matrix at each node is a 2×2 orthogonal matrix parameterized by a single reflection coefficient r ∈ [0,1].

**Delay quantization artifact:**
Each edge delay is quantized to integer samples. Maximum pitch error = 1200 * log₂(L/(L-1)) cents where L is the delay length. At 48kHz playing C4 (261Hz), L ≈ 23 samples per edge for 8 nodes, giving ~75 cent maximum error. This is significant — fractional delay interpolation (already implemented via linear interpolation) reduces this.

**Energy stability constraint:**
The scattering matrix must satisfy |S|₂ ≤ 1 (operator norm). With our parameterization [1-r, r; r, 1-r], the eigenvalues are 1 and 1-2r. For r ∈ [0,0.5], both eigenvalues are in [0,1] — energy is preserved or dissipated. For r > 0.5, the matrix amplifies — but damping (0.998 per sample) absorbs this.

**Aliasing from corruptions:**
Nonlinearities create harmonics. If the lattice frequency is f₀ and corruption creates the kth harmonic, kf₀ must be < sr/2 to avoid aliasing. At 48kHz, a 440Hz note with softclip generates harmonics up to ~5kHz (safe). Fold generates harmonics up to ~20kHz (borderline). We accept this trade-off over internal oversampling for CPU efficiency.

---

## 2026-03-28: Visual language evolution

### What we learned about BespokeSynth's UI

The existing UI has a hidden drop shadow system (written, disabled). Someone built it and turned it off — probably for performance or aesthetic reasons. We turned it back on with gentler parameters and it works.

The rendering pipeline is immediate-mode through two layers (OpenFrameworks + NanoVG). NanoVG handles effects (gradients, shadows, blur). OF handles shapes. They compose naturally because OF internally uses NanoVG.

### The principle: draw the DSP, not a metaphor

The Karplus-Strong string visualization draws the actual delay buffer — not a visualization of a string, but the string IS the buffer. The LatticeSynth visualization draws the actual node states — not a diagram of a lattice, but the lattice IS the DSP.

This is the design principle: **the visualization should be isomorphic to the computation**. Not a metaphor. Not an analogy. The same mathematical object, rendered.

---

## 2026-03-28: CohomologySynth — from simplicial complexes to sound

### The mathematical ascent

LatticeSynth lives in π₁ — the fundamental group. It counts loops, classifies winding. But higher cohomology yearns to be heard.

CohomologySynth lives in H*(K;R) — the full cohomology ring of a simplicial complex K. The key objects:

**Cochains** are the signal carriers:
- C⁰ (vertex cochains) = amplitudes at nodes — what you hear
- C¹ (edge cochains) = flows along connections — how energy moves
- C² (face cochains) = pressure in enclosed regions — what resonates

**The coboundary operator δ** propagates signal upward:
- δ⁰: takes a vertex function and computes its gradient on edges
- δ¹: takes an edge flow and computes its curl around faces

**The Hodge Laplacian Δ = δδ* + δ*δ** is the wave equation on the complex. Its eigenvalues ARE the resonant frequencies. Its eigenvectors ARE the mode shapes. The Hodge theorem says: harmonic forms (ker Δ) are isomorphic to cohomology groups.

### What the Betti numbers sound like

β₀ counts connected components. For a connected complex, β₀ = 1 — one fundamental. For a disconnected complex, each component has its own fundamental.

β₁ counts independent loops. Each loop creates a resonant mode that doesn't exist in a tree. A torus (β₁ = 2) has two independent loop resonances. A bouquet of circles (β₁ = 4) has four.

β₂ counts enclosed cavities. A tetrahedron (β₂ = 1) has one cavity mode — a breathing resonance where the enclosed volume oscillates. This is a formant.

### The DSP pipeline (precise)

1. **Build complex** — specify vertices, edges, faces
2. **Compute δ⁰** — N₁ × N₀ matrix: δ⁰[e][v] = ±1 for boundary vertices of edge e
3. **Compute δ¹** — N₂ × N₁ matrix: δ¹[f][e] = ±1 for boundary edges of face f
4. **Form Laplacian** — Δ₀ = (δ⁰)ᵀ δ⁰ (graph Laplacian, N₀ × N₀)
5. **Eigendecompose** — Jacobi iteration, O(N³) but N ≤ 12 so instant
6. **Extract modes** — each nonzero eigenvalue λᵢ gives frequency √λᵢ
7. **Excite** — project note velocity onto modes via eigenvector at excite vertex
8. **Synthesize** — sum of damped sinusoids at mode frequencies, O(modes) per sample

**Why this is better than additive synthesis**: the mode frequencies and relative amplitudes aren't arbitrary — they come from the topology. A tetrahedron sounds like a tetrahedron. An octahedron sounds different because it IS different.

### The presets and what they sound like

| Preset | V | E | F | β₀ β₁ β₂ | Character |
|--------|---|---|---|-----------|-----------|
| Triangle | 3 | 3 | 1 | 1 0 0 | Simple, three-mode, bell-like |
| Tetrahedron | 4 | 6 | 4 | 1 0 1 | Hollow, with cavity formant |
| Octahedron | 6 | 12 | 8 | 1 0 1 | Richer, more modes, sphere-like |
| Torus | 7 | 14+ | 14 | 1 2 1 | Two loop resonances + cavity |
| Klein bottle | 6 | ~12 | 7 | 1 1 0 | One loop, no cavity — odd |
| Bouquet | 5 | 8 | 0 | 1 4 0 | Many loops, no cavity — stringy |
| Suspension | 6 | 12 | 8 | 1 0 1 | Sphere-like, clear bell |

### The visualization

The simplicial complex is drawn as its 2D projection:
- **Faces** (2-simplices): filled triangles, opacity pulsing with face pressure
- **Edges** (1-simplices): lines, brightness scaling with energy flow
- **Vertices** (0-simplices): glowing dots, displaced radially by amplitude
  - Positive amplitude: green glow (category color)
  - Negative amplitude: blue glow (phase-shifted)
- **Mode spectrum bar**: horizontal strip at bottom showing active mode energies
- **Betti numbers**: displayed as β = (β₀, β₁, β₂) with simplex counts

### What's rubbery about it

The vertices MOVE. They're displaced radially by their amplitude in the current mode superposition. The faces breathe — their opacity pulses with the average vertex displacement. The edges flex as the energy flows through them. The whole structure wobbles like a rubber object vibrating at its natural frequencies.

This is what "rubbery rings and higher-dimensional shapes, awkwardly sliced glued surgeried and projected" looks like when rendered at 60fps and sonified through eigenmodes.
