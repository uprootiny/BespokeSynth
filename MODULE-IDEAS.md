# Module Ideas — from the topology synthesis framework

What else can we build using the same mathematical core (lattices, cohomology, waveguides, modal synthesis) as DSP and visualization engines?

---

## Effects (audio processors — take audio in, transform it, send it out)

### 1. TopologyFilter
**What:** An audio effect that routes input audio through a waveguide lattice as a resonant filter. The lattice topology shapes which frequencies pass and which are absorbed.

**DSP:** Input signal excites the lattice continuously (not just on note-on). The lattice resonates at its eigenfrequencies — frequencies near the lattice modes pass through amplified, others are attenuated. Pitch tracking (from note input) tunes the lattice to follow the melody.

**Controls:** topology (ring/fixed/mobius), node count (resonance density), damping (bandwidth), reflection (depth), corruption (harmonic distortion of the filter itself)

**Visualization:** Same lattice as TopologySynth but with audio flowing through it visibly. Input waveform enters from the left, resonant frequencies glow at corresponding nodes, output exits right.

**Musical use:** Like a comb filter but with configurable topology. Ring = metallic resonance. Fixed = room-like. Möbius = octave-doubling flanges.

---

### 2. SpectralMesh
**What:** An effect that decomposes audio into FFT bins, maps them onto a 2D simplicial complex, processes them through the complex's Laplacian diffusion, and resynthesizes.

**DSP:** FFT → assign bins to vertices of a mesh → apply Laplacian smoothing (diffuse spectral energy between neighboring bins) → IFFT. The mesh topology determines how spectral energy spreads. A ring mesh smears equally. A tree mesh creates formant-like peaks. A disconnected mesh (high β₀) isolates frequency bands.

**Controls:** mesh topology preset, diffusion rate (how fast spectral energy spreads), mesh density (how many bins per vertex), bypass blend

**Visualization:** 2D mesh of colored dots, each dot's brightness = spectral energy at that bin. Watch the spectrum diffuse across the mesh in real time.

**Musical use:** Spectral blur, vowel shaping, frequency-domain reverb. Unlike a traditional EQ, the relationships between frequencies are determined by graph topology, not just curves.

---

### 3. CohomologyVerb (reverb)
**What:** A reverb where the reflection structure comes from a simplicial complex. Each face is a reflective surface. Each edge is a propagation path. Each vertex is a scattering point. The Betti numbers determine the reverb character.

**DSP:** Feedback delay network (FDN) where the delay lengths come from the edge weights of the complex and the mixing matrix comes from the Laplacian. β₁ controls how many independent recirculation loops exist (loop density = diffusion). β₂ controls how many enclosed spaces exist (cavity resonances = coloration).

**Controls:** complex preset (room, hall, cathedral, cave, alien), size (scales all delays), damping (HF absorption), diffusion (β₁-weighted), coloration (β₂-weighted)

**Visualization:** The simplicial complex as a 3D wireframe projected to 2D, with reverb energy flowing as pulses along edges and pooling as glows in faces.

**Musical use:** Reverb whose character comes from topology, not just delay times. A tetrahedron reverb sounds fundamentally different from a torus reverb — not just shorter or longer, but structurally different in how reflections interact.

---

## Synths (note-driven sound generators)

### 4. MembraneSynth
**What:** A 2D waveguide mesh that models a vibrating membrane (drum head, plate, gong). The mesh is a triangulated disk or rectangle. Strike position determines which modes are excited.

**DSP:** 2D digital waveguide mesh on a triangulated surface. Each interior node scatters to its neighbors. Boundary nodes reflect (clamped or free edge). Modal spectrum is inharmonic (Bessel function zeros for circular membranes) — this is why drums don't have a clear pitch.

**Controls:** shape (circle, square, irregular), size (pitch), tension (affects mode spacing), strike position (x,y on the membrane), damping, material (steel=high modes, skin=low modes)

**Visualization:** Top-down view of the membrane. Color = displacement. Nodal lines visible as contours. Strike point shown as a ripple origin. Chladni-like patterns emerge naturally.

**Musical use:** Drums, timpani, tabla, gongs, plates. The inharmonic spectrum is what makes percussion sound like percussion. Strike position = timbre (center = boom, edge = ring, off-center = complex).

---

### 5. CoupledOscillators
**What:** N spring-coupled harmonic oscillators. Energy transfers between oscillators create beating, phasing, and complex timbral evolution over time.

**DSP:** System of coupled ODEs: m*x''_i + d*x'_i + k*x_i = Σ_j c_ij*(x_j - x_i). Solved via Verlet integration. Each oscillator has its own natural frequency. Coupling strength determines how fast energy transfers. Beating period = 1/(f_i - f_j).

**Controls:** N oscillators (2-8), per-oscillator frequency ratio, coupling strength, damping, excitation (which oscillators to pluck)

**Visualization:** N masses on springs drawn as circles connected by wiggly lines. Mass displacement shown as vertical position. Spring deformation shown as stretch/compression with color coding. Watch energy slosh between oscillators in real time.

**Musical use:** Gamelan, church bells, prepared piano. The characteristic "beating" and "shimmering" of metallic percussion comes from coupled inharmonic modes. Also useful as a slow LFO-like modulation source (the beating pattern IS a complex waveform).

---

### 6. FundamentalGroupSynth
**What:** A synth where you literally draw the fundamental group presentation (generators and relations) and it sonifies the resulting space.

**DSP:** The user specifies generators (loops) and relations (how loops compose). The module constructs the Cayley graph, computes its Laplacian spectrum, and synthesizes via modal summation. Different groups have radically different spectra: Z gives harmonic series, Z² gives a 2D lattice of modes, free groups give trees (no resonance loops = all modes decay).

**Controls:** group type (cyclic Z/n, free abelian Z^k, dihedral D_n, symmetric S_n, free group F_n), order parameter, excitation

**Visualization:** The Cayley graph of the group, with generators as colored edges. Active modes light up the corresponding graph elements.

**Musical use:** Pure mathematical sonification. Each group has a unique sound. Cyclic groups = the familiar harmonic series. Dihedral groups = bell-like inharmonic spectra. Free groups = noise-like (no resonance structure). This is genuinely new — nobody has built a synth parameterized by group presentations.

---

## Modulators / Utilities

### 7. TopologyLFO
**What:** An LFO whose waveform is generated by a small lattice. Instead of sine/triangle/square, the waveform is the natural oscillation of a 3-4 node waveguide system. Different topologies give different LFO shapes.

**DSP:** Same lattice engine as LatticeSynth but running at sub-audio rates (0.1-20 Hz). Output is the displacement at a single node. Ring = smooth cycling. Fixed = bouncing. Möbius = alternating sign on each cycle.

**Controls:** rate, topology, node count, which node to output, depth

**Visualization:** Tiny lattice with one highlighted output node. The output waveform shown as a trailing line.

**Musical use:** Modulation source that produces waveforms no standard LFO can. The Möbius LFO naturally produces a waveform that alternates between two shapes — like a two-phase LFO in one module.

---

### 8. GraphRouter
**What:** A routing module where the audio/note signal path is defined by a graph. Patch multiple sources and destinations, and the graph topology determines how signals mix, split, and feedback.

**DSP:** N input channels, M output channels, connected by a configurable bipartite graph. Edge weights control mix levels. Optional feedback paths (with delay to prevent instability). The adjacency matrix IS the mix matrix.

**Controls:** graph editor (drag to create/remove edges), per-edge gain, feedback enable, delay per feedback edge

**Visualization:** Bipartite graph with sources on left, destinations on right, weighted edges between. Signal level shown as edge brightness. Feedback loops highlighted in a different color.

**Musical use:** Replaces a pile of send/return/mixer modules with one visual graph. Especially powerful for feedback patches where you need to see the signal flow clearly.

---

## Priority for implementation

| Module | Effort | Sound value | Visual value | Novelty |
|--------|--------|-------------|-------------|---------|
| **TopologyFilter** | 4h | Very high | High | High |
| **MembraneSynth** | 8h | Very high | Very high | Medium |
| **CoupledOscillators** | 4h | High | Very high | Medium |
| **CohomologyVerb** | 8h | Very high | High | Very high |
| **TopologyLFO** | 2h | Medium | Medium | High |
| **SpectralMesh** | 12h | High | Very high | Very high |
| **FundamentalGroupSynth** | 6h | Medium | High | Extremely high |
| **GraphRouter** | 6h | High | High | Medium |

**My recommendation:** TopologyFilter first (reuses 90% of existing LatticeSynth DSP, just needs to accept audio input), then CoupledOscillators (simple physics, beautiful visualization, distinct sound), then MembraneSynth (biggest visual impact, musically the most useful new percussion tool).
