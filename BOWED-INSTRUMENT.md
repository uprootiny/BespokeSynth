# Bowed Instrument — Mathematical Architecture

A physically plausible violin requires five coupled subsystems. Each is a distinct mathematical object. The coupling between them is where the sound lives.

---

## 1. The String (waveguide)

**Math:** 1D wave equation on [0, L] with velocity-dependent boundary conditions.

**What we already have:** LatticeSynth's waveguide delay lines. But a bowed string is NOT a plucked string. The difference:
- **Pluck**: impulsive excitation, then free vibration (Karplus-Strong)
- **Bow**: continuous excitation via stick-slip friction at a contact point

The string state is two traveling waves (a⁺, a⁻) meeting at the bow contact point. The bow applies a nonlinear friction force that depends on the relative velocity between bow and string.

**Key equation:**
```
v_rel = v_bow - v_string(x_bow)
f_friction = f_N * mu(v_rel)
```
where `mu(v)` is the friction curve — the Friedlander function:
```
mu(v) = {
  mu_s * sign(v)           if |v| < v_break  (sticking)
  mu_d * sign(v) * (v_break/|v|)^alpha  otherwise  (slipping)
}
```
`mu_s` = static friction (~0.8), `mu_d` = dynamic friction (~0.3), `alpha` ≈ 0.4.

This is the stick-slip oscillation that makes bowed strings sing. The string periodically sticks to the bow (moving with it), then breaks free and snaps back. Helmholtz showed this creates a triangular wave traveling around the string — the "Helmholtz corner."

**Implementation:**
- Two delay lines (nut-to-bow, bow-to-bridge), connected at the bow point
- Friction force computed at the bow point each sample
- Bow velocity = user-controlled parameter (maps to dynamics)
- Bow position = user-controlled (timbre: closer to bridge = more harmonics)
- Bow pressure = user-controlled (maps to f_N, the normal force)

---

## 2. The Body (resonator)

**Math:** The violin body is a 3D shell with complex mode shapes. But we can model it as a bank of resonant filters — each mode characterized by frequency, bandwidth, and amplitude.

**What makes a violin sound like a violin** is NOT the string (all bowed strings sound similar in isolation). It's the body resonance filtering the string's output. The body has several named resonances:

| Mode | Freq (approx) | Name | Character |
|------|--------------|------|-----------|
| A0 | 275 Hz | Air mode | Helmholtz resonance of the body cavity |
| T1 | 460 Hz | Top plate | First bending mode of the top plate |
| C2 | 530 Hz | Corpus | Second cavity mode |
| B1- | 390 Hz | Body | Lower body mode |
| B1+ | 510 Hz | Body | Upper body mode |

These are the violin's formants — they stay fixed regardless of which note is played, just like vowel formants in speech.

**Implementation:**
- Bank of 5-8 biquad bandpass filters in parallel, summed
- Frequencies and Q values from published violin body measurements
- The bridge transfers string vibration to the body — this is a simple gain + lowpass
- Our CohomologySynth's modal approach works here: the body IS a simplicial complex with eigenmodes

**Connection to our topology framework:**
The body modes are the eigenvalues of the Laplacian on the body's surface mesh. β₂ = 1 (the body is a closed shell with one enclosed cavity = the air mode A0). The air mode IS β₂.

---

## 3. The Bow (exciter)

**Math:** The bow is a continuous excitation source parameterized by:
- `v_bow`: bow velocity (controls loudness and sustain)
- `f_bow`: bow force / pressure (controls tone quality — light = flautando, heavy = crunchy)
- `x_bow`: bow position on string (0 = bridge, 1 = fingerboard)

The friction model is the heart of the instrument. The simplest correct model is the "hyperbolic" friction curve:

```
mu(v_rel) = mu_d + (mu_s - mu_d) * v_break / (|v_rel| + v_break)
```

This is smooth (differentiable), avoids the discontinuity of Coulomb friction, and produces correct Helmholtz motion. It's also cheap — one division per sample.

**Bow noise:** Real bow hair is not perfectly smooth. Adding filtered noise to `v_bow` creates the characteristic "rosin" texture:
```
v_bow_actual = v_bow + noise_amplitude * bandpass_noise(200-2000 Hz)
```

---

## 4. The Bridge (coupling)

**Math:** The bridge is a mechanical impedance transformer. It converts the string's transverse vibration into the body's longitudinal vibration. Modeled as a second-order transfer function:

```
H_bridge(s) = k * s² / (s² + 2*zeta*w_b*s + w_b²)
```

where `w_b` ≈ 2π * 3000 Hz (bridge resonance) and `zeta` ≈ 0.5 (moderately damped).

The bridge is a highpass filter — it emphasizes upper harmonics from the string. This is why the bridge position and material matter so much in real instruments.

**Implementation:** A single biquad highpass at the bridge frequency, between string output and body input.

---

## 5. Sympathetic Strings (coupling resonators)

**Math:** A real violin has 4 strings, and they resonate sympathetically. When you bow one string, the others vibrate at their natural frequencies if those frequencies align with harmonics of the bowed string.

This is coupled oscillation: weak coupling through the bridge/body transfers energy between strings at shared harmonic frequencies.

**Implementation:**
- 4 independent waveguide strings, each tuned (G3, D4, A4, E5)
- All strings connected to the same body resonator
- Sympathetic coupling: body output feeds back into all strings at low amplitude
- The coupling strength determines how much "ring" and "bloom" the instrument has

---

## Module Design: `BowedString`

### Parameters

| Group | Parameter | Range | Maps to |
|-------|-----------|-------|---------|
| **Bow** | velocity | 0-1 | v_bow (loudness/sustain) |
| | pressure | 0-1 | f_N (light → heavy tone) |
| | position | 0.05-0.2 | x_bow / L (bridge→fingerboard) |
| | noise | 0-0.1 | bow hair texture |
| **String** | (from MIDI pitch) | | fundamental frequency |
| | damping | 0.99-0.9999 | string decay |
| | brightness | 0-1 | string HF content |
| **Body** | size | 0.5-2.0 | scales all body mode frequencies |
| | resonance | 0.1-0.99 | Q of body modes |
| | air coupling | 0-1 | strength of A0 (air) mode |
| **Sympathetic** | coupling | 0-0.3 | string-to-string via body |
| | strings | 1-4 | number of sympathetic strings |

### Signal Flow

```
MIDI note → set string pitch

Bow parameters → friction force at bow point
                 ↓
String waveguide ←→ bow interaction (stick-slip)
        ↓
Bridge filter (biquad highpass ~3kHz)
        ↓
Body resonator (5-8 parallel bandpass modes)
        ↓                    ↑
Output             Sympathetic coupling back to strings
```

### Visualization

The most natural visualization for a bowed string:
- **The string:** horizontal, showing the traveling waves and the Helmholtz corner
- **The bow:** vertical line crossing the string at bow position, with contact force shown as thickness
- **Body modes:** small spectrum-like bars below the string showing which body resonances are active
- **Sympathetic strings:** thinner lines below the main string, vibrating when harmonics align

The Helmholtz corner is the signature visual: a sharp kink traveling around the string, flipping direction at the bridge and nut. When the bow sticks, the corner is at the bow. When it slips, the corner races away. This is literally what you see on a real bowed string in slow motion.

---

## Why this connects to our topology framework

The bowed instrument is a **coupled system of topologically distinct spaces**:

1. **String:** 1D interval [0,L] — trivial topology (π₁ = {e})
2. **Body:** 2D closed shell — sphere-like (β₂ = 1, the air cavity)
3. **Sympathetic coupling:** 4 strings coupled through the body = a graph with 4 edges meeting at a vertex (the bridge)

The bridge IS a 0-simplex connecting 1-simplices (strings) to a 2-simplex (body). The whole instrument is a simplicial complex with β₀ = 1 (connected), β₁ = 3 (three independent string-to-body loops beyond the minimum spanning tree), β₂ = 1 (the body cavity).

This is not metaphorical. The instrument's resonant behavior is determined by the Hodge Laplacian on this complex. The topology determines which frequencies couple, which don't, and where energy accumulates.
