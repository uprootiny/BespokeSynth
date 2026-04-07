# Algorithmic Quandaries, Tricks, and Missed Opportunities

An honest inventory of the DSP algorithms across all 13 modules — what's correct, what's approximate, and what could be better.

---

## What We Do Right

### Unitary scattering (LatticeSynth, TopologySynth, TopologyFilter)
The Kelly-Lochbaum parameterization S = [cos θ, sin θ; sin θ, -cos θ] guarantees energy preservation for any θ. This is strictly correct — no parameter value can cause the lattice to create energy. The older (1-r, r; r, 1-r) matrix that we replaced had an eigenvalue > 1 for r > 0.5, which would blow up.

### Thiran allpass delay (LatticeSynth)
First-order allpass interpolation has flat magnitude response — it doesn't color the sound like linear interpolation does. The one-sample state variable is properly tracked per edge and cleared on note-on. This matters most for short delays (high pitches) where linear interp acts as a lowpass.

### Verlet integration (CoupledOscillators)
Störmer-Verlet is symplectic — it conserves energy over long times. This is why the coupled oscillators sustain cleanly instead of either blowing up (forward Euler) or losing energy (backward Euler). The sr/4 frequency clamp prevents the Verlet stability limit from being exceeded.

### FDTD stability (MembraneSynth)
The Courant number c² is clamped at 0.499 — the strict stability limit for 2D FDTD on a rectangular grid is c² < 0.5. This prevents the simulation from exploding.

### Householder FDN matrix (CohomologyVerb)
H = I - 2/N * ones(N,N) is orthogonal for any N. This means the reverb mixing matrix preserves energy at every reflection — the decay comes only from the explicit damping, not from matrix scaling. Householder is the simplest unitary N×N mixing matrix.

---

## What We Do Approximately (and Could Do Better)

### Linear interpolation everywhere except LatticeSynth
TopologySynth, TopologyFilter, TopologyDelay, BowedString all use delay lines but DON'T use allpass interpolation — they use either integer-sample delays or no interpolation at all. This means their pitch accuracy degrades for high notes.

**Fix:** Port the Thiran allpass from LatticeSynth to all modules using delay lines. This is a copy-paste + add state variables. Effort: 2 hours.

### FM stability is ad-hoc (FMCluster)
We limit the modulation matrix spectral radius implicitly (output clamp at ±2) but don't actually check stability. At high brightness + high feedback, the oscillators can hit the clamp every sample, which sounds like hard clipping.

**Better approach:** Compute the spectral radius of the modulation matrix D and scale it so max eigenvalue < some safe threshold (~8). This allows high modulation without hard clipping. Or: use a soft limiter (tanh) per operator instead of hard clamp.

### Body modes are static (BowedString)
The 6 body modes use fixed frequencies from published violin data. But the body resonances should shift with body size parameter. Currently mBodySize scales the frequencies, but the Q values and gains don't change — a larger body should have lower Q (more damping per mode) and different gain ratios.

**Fix:** Make Q and gain functions of frequency: Q = Q_base * (f / f_reference)^0.3, gain = gain_base * (f_reference / f)^0.5. This is the empirical scaling law for wooden resonators.

### Jacobi convergence is overkill (CohomologySynth)
We run 200 Jacobi sweeps but for N ≤ 12 it typically converges in 10-20 sweeps. The convergence check (off-diagonal sum < 1e-12) is correct but we compute it every sweep, which is O(N²) per sweep.

**Better:** Track the maximum off-diagonal element (one variable, updated during rotations) instead of recomputing the full off-diagonal sum. Early-exit when max < threshold. Saves ~80% of the computation for converged matrices.

### Fibonacci comb feedback path (FibonacciComb)
The current feedback mixes the wet signal back into the delay buffer at half the feedback gain. But the wet signal already includes ALL taps weighted by feedback^tap_index. This creates a geometric series of series — the effective feedback at the first tap is feedback * 0.5, at the second it's feedback² * 0.5 * feedback, etc. This is approximately correct but not the canonical comb filter feedback topology.

**Better:** Feed back ONLY the last (deepest) Fibonacci tap, not the full wet sum. This creates a true comb with resonant peaks at exactly the Fibonacci positions. The current approach smears the peaks.

---

## What We Miss Entirely

### No interpolation between topologies
When you change the topology dropdown (fixed → ring → Möbius), the lattice snaps instantly. There's no crossfade between the old and new topology. This creates a click.

**What we should do:** Morph between topologies over ~100ms. For boundary condition changes, interpolate the boundary reflection coefficient from the old value to the new. For node count changes, this is harder — you'd need to create/destroy delay lines without disrupting the current audio.

### No pitch tracking in effects
TopologyFilter and FibonacciComb accept note input for pitch tracking, but TopologyDelay and CohomologyVerb don't. The delay time should track MIDI pitch so the delay is always a musical interval of the played note.

### No dezipper on parameter changes
When you move a slider, the parameter jumps to the new value immediately. For continuous parameters in the audio path (volume, damping, reflection), this creates clicks. BespokeSynth's `ComputeSliders()` handles per-sample interpolation for some controls, but our custom parameters bypass this.

**Fix:** Use BespokeSynth's built-in slider smoothing or add our own exponential smoothing: `param_actual += (param_target - param_actual) * 0.01f` per sample.

### No oversampling for nonlinearities
The corruption functions (tanh, fold, rectify) and the FM synthesis create harmonics above Nyquist that fold back as aliasing. At moderate settings this is inaudible, but at high drive/brightness it's very audible — especially the fold function, which creates dense harmonics.

**Fix:** 2x oversampling around the nonlinearity: upsample → apply nonlinearity → downsample. Cost: ~2x for the nonlinear section only. For FM synthesis, the entire operator bank needs oversampling since every operator is a nonlinear source.

### No anti-aliased oscillators (FMCluster)
The FM operators use `sinf()` which is aliased above ~10kHz. For low ratios this is fine, but operators with ratio > 8 at high MIDI pitches produce aliasing.

**Fix:** Use polynomial-approximated bandlimited oscillators (polyBLEP for non-sine waveforms, or precomputed wavetable for sine). For sine specifically, `sinf()` is actually alias-free — the aliasing comes from the FM modulation creating non-sinusoidal effective waveforms.

---

## Algorithmic Tricks We Should Adopt

### 1. Delay-free loop for scattering junctions
Our waveguide propagation uses a snapshot-then-propagate approach. This works but introduces a one-sample latency per node. A delay-free loop approach uses matrix inversion to solve the scattering equations simultaneously, eliminating the latency. For small lattices (N ≤ 8) the matrix is tiny and the inversion is cheap.

### 2. Efficient eigendecomposition cache
CohomologySynth recomputes the entire eigendecomposition when the topology changes. But most topology changes (adding/removing one vertex or edge) only perturb the Laplacian slightly. Rank-1 update formulas can adjust eigenvalues incrementally for O(N²) instead of O(N³).

### 3. SIMD for per-sample loops
All our per-sample DSP loops are scalar. The membrane update (2D FDTD) is the heaviest — it touches N² floats per sample. SIMD vectorization (using JUCE's `FloatVectorOperations` or direct SSE/NEON) could speed this up 4x.

### 4. Modal synthesis with phase quadrature
CohomologySynth uses `sinf(phase)` for each mode. Phase quadrature maintains both sin and cos state and rotates them: `sin_new = sin*c - cos*s; cos_new = sin*s + cos*c`. This replaces one `sinf()` (expensive) with two multiplies and two adds (cheap). For N modes, this saves N transcendental function calls per sample.
