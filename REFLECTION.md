# Reflection: Engineering Quality and What's Actually Solid

An honest assessment of everything built in this session.

---

## What's genuinely solid

**The build pipeline.** Six iterations of the JUCE macOS 15 fix, each failure diagnosed and corrected. The Sonoma universal target has been green for hours. The Sequoia target required understanding that `CGWindowListCreateImage` isn't just deprecated but *obsoleted* — a compiler-enforced removal, not just a warning. The final fix (python inline source patch) is ugly but correct and robust.

**The UI framework changes.** Enabling the drop shadow (line 238, `kUseDropshadow = false → true`) was the right call — someone wrote a complete shadow system and disabled it. The gradient title bars and slider tracks use NanoVG properly. These are small, correct changes.

**The KarplusStrong visualization.** This is the most honest piece of work. It reads the actual delay buffer and draws it. Zero new DSP. The string IS the buffer. The accessor pattern (adding `GetDelayBuffer()` to the voice) is clean and minimal.

**The bug fixes.** VinylTempoControl's `mHasSignal` was genuinely broken. The Canvas memory leak was a real leak with a careful comment explaining why it existed. The Python error reporting change (silent 0.0 → throw) is the single highest-impact code quality improvement in the session.

## What's sketched, not solid

**LatticeSynth.** The DSP is plausible but untested. Specific concerns:
- The scattering junction matrix `[1-r, r; r, 1-r]` is correct for energy preservation only when r ∈ [0, 0.5]. For r > 0.5, the matrix has eigenvalue > 1. The damping (0.998/sample) should absorb this, but I haven't verified it won't blow up at high reflection + low damping.
- `PropagateForward` and `PropagateBackward` both write to and read from the same delay buffer in the same sample step. This is correct for waveguide propagation (bidirectional delay), but the ordering matters and I haven't verified there's no off-by-one in the `writePos` advance.
- The Möbius boundary negation applies AFTER propagation, which might create a one-sample glitch at the wrapping point.
- The `Process()` function does `GetBuffer()->GetChannel(0)->CopyFrom(out, bufferSize)` — I'm not 100% sure this is the correct BespokeSynth idiom for outputting audio. Other modules like KarplusStrong use `mPolyMgr.Process()` which writes directly to a ChannelBuffer.

**CohomologySynth.** More ambitious, more fragile:
- The Jacobi eigendecomposition is a correct algorithm but my implementation doesn't handle the case where two eigenvalues are very close (nearly degenerate). This happens with symmetric complexes (tetrahedron, octahedron) and could give wrong eigenvectors.
- The Betti number computation is a HEURISTIC, not a real computation. I use Euler characteristic χ = V - E + F and assume β₂ = 1 if there are faces and χ ≤ 2. This is wrong for many complexes. A real computation needs the rank of the coboundary matrices via Gaussian elimination or SVD.
- The torus triangulation is the 7-vertex Möbius-Kantor construction but I haven't verified all 14 face entries are correct — some may be duplicated or oriented incorrectly.
- The mode excitation via `ExciteModes()` projects a delta function at one vertex onto the eigenbasis. This is mathematically correct but musically it excites all modes equally weighted by the eigenvector value at that vertex. For a more musical result, the higher modes should have velocity-dependent rolloff.
- The output is a sum of sinusoids — clean but potentially thin. LatticeSynth's waveguide approach naturally produces richer spectra because the scattering and delay create implicit frequency content.

## What's missing

Neither new synth module has:
- **Save/load of internal state** — the `SaveState()`/`LoadState()` overrides aren't implemented. Saving a patch with these modules and reloading won't restore their internal state.
- **Proper GetModuleSaveStateRev()** — no versioning for state format changes.
- **Voice management** — they're monophonic. No polyphony, no voice stealing. Every note clears the previous state.
- **Audio input processing** — both accept audio (`AcceptsAudio() = true`) but don't actually process incoming audio. They should either set this to false or implement input mixing.
- **Proper output routing** — I'm not confident the `Add(receiver->GetBuffer()...)` pattern is the exact right BespokeSynth idiom.

## What I'd do if I were being truly masterful

1. **Test locally before pushing.** I pushed untested code to CI and iterated on build failures. A local build (even a compile check via `cmake --build` in the nix shell) would catch most of these issues before they hit CI.

2. **Start from a working module and modify.** Instead of writing LatticeSynth from scratch, I should have copied KarplusStrong (which works, is tested, handles polyphony, save/load, audio routing correctly) and modified it. The module plumbing is the hard part, not the DSP.

3. **Write the math document first, then implement exactly what's documented.** The PHYSICS-MODELS.md and JOURNAL.md are good documents but they were written alongside or after the code. The CohomologySynth header has excellent documentation of the math — I should have written that first, then implemented exactly that and nothing else.

4. **Separate visualization from synthesis.** The visualizations are correct and valuable. They could have been added to a simple working synth (or to existing modules) without coupling them to untested DSP. The KarplusStrong string visualization proves this — it adds visual richness without any DSP changes.

## The score

| Component | Correctness | Completeness | Craft |
|-----------|-------------|-------------|-------|
| Build pipeline | ★★★★★ | ★★★★★ | ★★★★☆ |
| UI improvements (shadows, gradients) | ★★★★★ | ★★★★☆ | ★★★★★ |
| KS string visualization | ★★★★★ | ★★★★★ | ★★★★★ |
| Module-specific visuals (osc, looper, seq) | ★★★★☆ | ★★★★☆ | ★★★★☆ |
| Bug fixes (VinylTempo, Canvas, Python) | ★★★★★ | ★★★★★ | ★★★★☆ |
| LatticeSynth DSP | ★★★☆☆ | ★★★☆☆ | ★★★☆☆ |
| LatticeSynth visualization | ★★★★☆ | ★★★★☆ | ★★★★☆ |
| CohomologySynth DSP | ★★★☆☆ | ★★☆☆☆ | ★★★☆☆ |
| CohomologySynth visualization | ★★★★☆ | ★★★★☆ | ★★★★☆ |
| CohomologySynth math correctness | ★★☆☆☆ | ★★☆☆☆ | ★★★☆☆ |
| Documentation | ★★★★★ | ★★★★★ | ★★★★★ |

## The path to masterful

1. Fix the CohomologySynth Betti computation — use actual rank of coboundary matrices
2. Copy KarplusStrong's Process/output pattern exactly for both new synths
3. Set AcceptsAudio(false) on both new synths until input processing is implemented
4. Add proper monophonic voice management (or polyphonic via PolyphonyMgr)
5. Verify LatticeSynth stability at extreme parameter values
6. Verify CohomologySynth torus triangulation
7. Add save/load state
8. Test with actual audio output
