# Physical Models for Audio DSP + Visual + Musical Semantics

Brainstorm: physics models that simultaneously work as DSP algorithms, have musical meaning, and yield beautiful visualizations in our NanoVG environment.

---

## 1. Taut String (Karplus-Strong → Wave Equation)

**Physics:** A string fixed at both ends vibrates as a superposition of standing waves. The fundamental frequency is f = v/(2L) where v = sqrt(T/μ). Plucking excites harmonics based on pluck position. Damping causes higher harmonics to decay faster.

**DSP:** Already implemented as Karplus-Strong (delay line + lowpass filter). Can be extended to:
- **Position-dependent plucking** — excite different harmonics by varying initial displacement shape
- **Bowing** — continuous excitation with velocity-dependent friction (stick-slip dynamics)
- **Sympathetic resonance** — multiple strings coupled through a bridge

**Visualization:**
- Draw the string as a live waveform between two fixed points (bridge nuts)
- Show standing wave nodes as faint vertical lines
- Highlight the pluck point with a radial glow
- Feedback parameter → visible decay rate of oscillation
- Filter cutoff → visible harmonic rolloff (smoother wave shape)
- **Bowing:** show the bow contact point as a moving pressure zone, stick-slip as jittery displacement

**Musical semantics:** Directly maps to guitar, violin, piano, sitar. Pluck position = timbre. Tension = pitch. Damping = sustain.

---

## 2. Resonant Cavity (Waveguide Mesh)

**Physics:** Sound in an enclosed space: reflections off walls create standing wave patterns (room modes). A 2D waveguide mesh models rectangular or irregular cavities. Eigenfrequencies depend on dimensions.

**DSP:**
- 2D digital waveguide mesh (network of bidirectional delay lines)
- Cavity shape → frequency response
- Absorption coefficients at walls → decay
- Excitation point → which modes are excited

**Visualization:**
- 2D pressure field as a heatmap (red/blue for +/- pressure)
- Animate wave propagation in real-time
- Show cavity walls as solid lines, openings as gaps
- Dragging the excitation point changes which modes are excited
- Wall absorption shown as color (reflective = bright, absorptive = dark)
- **NanoVG:** Use a grid of colored rects or a nanoVG image fill updated per frame

**Musical semantics:** Models singing voice (throat cavity), drum body, room acoustics, wind instruments (flute bore). Cavity shape = vowel formant.

---

## 3. Membrane (Drum Head)

**Physics:** 2D wave equation on a circular membrane. Vibrational modes are Bessel functions J_mn. The spectrum is inharmonic — partials are NOT integer multiples of the fundamental.

**DSP:**
- Modal synthesis: sum of decaying sinusoids at Bessel zeros
- Strike position determines which modes are excited
- Tension → pitch (but inharmonic)
- Damping → per-mode decay rates

**Visualization:**
- Draw the circular membrane as a deforming surface (2D top-down view)
- Show nodal patterns (Chladni figures) as faint lines
- Strike point as a radial impulse animation
- Mode amplitudes as concentric color rings
- **NanoVG:** `nvgRadialGradient` for each active mode, composited

**Musical semantics:** Drum, tabla, timpani. Strike position = timbre character. Tension = pitch. Inharmonicity is THE defining character of drums.

---

## 4. Reed / Lip Reed (Clarinet, Brass)

**Physics:** Nonlinear coupling between oscillating reed and bore acoustics. Reed position modulates airflow, which in turn drives reed vibration. Creates self-sustaining oscillation at the bore's resonant frequency.

**DSP:**
- Digital waveguide bore (delay line pair) + nonlinear reed function
- Blowing pressure → loudness and onset behavior
- Reed stiffness → timbre (bright/dark)
- Bore length → pitch
- Register hole → octave jumps

**Visualization:**
- Show the bore as a tube with pressure waves traveling back and forth
- Reed as a vibrating flap at one end
- Blowing pressure as an animated airflow particle stream
- Standing wave pattern visible inside the bore
- Register transitions shown as the wave pattern snapping to a higher mode
- **NanoVG:** Draw bore as rounded rect, waves as animated sine curves inside

**Musical semantics:** Clarinet, saxophone, trumpet, oboe. Breath = expression. Embouchure = reed stiffness. Overblowing = registers.

---

## 5. Coupled Oscillators (Gamelan, Bell)

**Physics:** Two or more oscillators coupled by springs/bridges. Energy transfers between oscillators, creating beating patterns and complex timbres. In bells, the wall vibrations couple through the structure.

**DSP:**
- N coupled harmonic oscillators: mass-spring system
- Coupling strength → beat frequency
- Each oscillator at its own natural frequency
- Inharmonic spectra arise naturally from the coupling

**Visualization:**
- Draw N masses connected by springs
- Animate displacement of each mass
- Spring deformation visible (stretched = warm color, compressed = cool)
- Coupling strength shown as spring thickness
- Beat patterns emerge as visible energy sloshing between oscillators
- **NanoVG:** Circles for masses, lines for springs, animate position per audio frame

**Musical semantics:** Gamelan, church bells, tuning forks, sympathetic strings. The "beating" and "shimmering" quality of metallophones comes from coupled inharmonic modes.

---

## 6. Fluid Dynamics (Flute, Whistle)

**Physics:** Airflow over an edge creates turbulence → jet oscillation → sound. The pipe resonance selects the oscillation frequency. Jet velocity determines which mode sounds.

**DSP:**
- Jet-drive model: delay feedback with jet nonlinearity
- Breath velocity → pitch register and brightness
- Embouchure (lip-to-edge distance) → onset threshold
- Tone hole position → effective tube length

**Visualization:**
- Show turbulent jet as a wavy line oscillating above an edge
- Pipe resonance as standing waves inside a cylinder
- Breath pressure as animated particle flow
- Harmonics as layered wave amplitudes
- **NanoVG:** Particle-like dots flowing along a path, nvgBezier for jet oscillation

**Musical semantics:** Flute, recorder, whistle, pan pipes, organ pipes. Breath IS the instrument.

---

## 7. String Lattice (Prepared Piano, Cymbal)

**Physics:** 2D lattice of coupled oscillators models plates and cymbals. Nonlinear coupling creates energy cascade to higher modes (the cymbal "wash" effect). In prepared piano, objects on strings add mass/damping at specific points.

**DSP:**
- Lattice of delay lines or modal synthesis
- Preparation objects = point masses/dampers in the lattice
- Hit location and force determine mode excitation
- Nonlinear coupling for energy cascade

**Visualization:**
- Draw the lattice as a grid of connected points
- Animate node displacement as color intensity
- Show preparation objects as colored dots on the lattice
- Wave propagation visible as ripples from strike point
- Energy cascade shown as high-frequency shimmer spreading outward

**Musical semantics:** Cymbal, gong, prepared piano, metallic percussion. The visual directly shows why cymbals have that spreading "wash" quality.

---

## Implementation Priority

For BespokeSynth, I'd prioritize based on:
1. **Visual impact** — how good will it look?
2. **DSP simplicity** — can we implement it without rewriting the audio engine?
3. **Musical value** — does it make sounds people want?

| Model | Visual | DSP | Musical | Priority |
|-------|--------|-----|---------|----------|
| **Taut string** (enhance KS) | High | Easy (exists) | High | **P1** |
| **Coupled oscillators** | High | Medium | High | **P2** |
| **Membrane** (drum) | Very high | Medium | High | **P2** |
| **Resonant cavity** | Very high | Hard | Medium | **P3** |
| **Reed** | Medium | Medium | High | **P3** |
| **Fluid** | Medium | Hard | Medium | **P4** |
| **String lattice** | Very high | Hard | Medium | **P4** |

### First: Visualize the existing Karplus-Strong string

The KS module already has the delay line — it's just invisible. We can:
1. Read the delay buffer
2. Draw it as a vibrating string between two fixed endpoints
3. Show the feedback/decay as visible amplitude reduction
4. Show the filter cutoff as visible harmonic rolloff
5. Highlight excitation events

This requires **zero DSP changes** — just drawing what's already there.
