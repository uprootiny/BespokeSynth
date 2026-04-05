# Resource Analysis — Memory, Compute, and Hardware Limits

Honest accounting of what our modules cost and where the risks are.

---

## Memory

| Module | Static RAM | What's eating it |
|--------|-----------|-----------------|
| **CohomologyVerb** | **1.7 MB** | 8 delay lines × 48000 floats × 4 bytes = 1.5MB |
| LatticeSynth | 513 KB | 16 nodes × 2 delay buffers × 4096 floats |
| TopologyDelay | 391 KB | 1 main delay (96K floats) + 4 node buffers |
| FibonacciComb | 188 KB | 48000-float delay buffer |
| BowedString | 130 KB | 4 strings × 2 delay buffers × 4096 floats |
| TopologySynth | 129 KB | 8 nodes × 2 delay buffers × 2048 floats |
| TopologyFilter | 128 KB | 8 nodes × 2 delay buffers × 2048 floats |
| CohomologySynth | 13 KB | Matrices + eigenvectors (N≤12, all dense) |
| MembraneSynth | 3.5 KB | 3 grids of 16×16 floats |
| FMCluster | ~1 KB | 6×6 modulation matrix + 6 operator states |
| CoupledOscillators | ~1 KB | 8 mass positions + velocities |

**Total for all 11 modules simultaneously: ~3.1 MB**

This is nothing. An M-series Mac has 8-24 GB of RAM. Even 100 instances of every module would use ~310 MB — well within limits. BespokeSynth itself uses ~50-100 MB at rest.

**The risk:** CohomologyVerb at 1.7 MB per instance. If someone spawns 10 of them, that's 17 MB of delay buffers. Still fine, but worth noting.

**The waste:** LatticeSynth allocates 16 × 4096 = 65536 floats per delay direction even when only using 3 nodes with delay lengths of ~100 samples. That's 99.6% wasted. A dynamic allocation would save memory but add complexity and potential allocation on the audio thread.

**Verdict:** Memory is not a concern on any modern Mac. Leave the static allocations — they're cache-friendly and allocation-free.

---

## Compute

### Per-sample cost (cycles, approximate)

| Module | Cycles/sample | % of budget* | Bottleneck |
|--------|--------------|-------------|-----------|
| **MembraneSynth (16×16)** | **~1536** | **2.3%** | 2D FDTD: 256 cells × 6 ops each |
| LatticeSynth (16 nodes) | ~480 | 0.7% | Allpass reads (2 per node) |
| TopologySynth (8 nodes) | ~400 | 0.6% | Lattice + modal + shaper |
| BowedString (4 strings) | ~400 | 0.6% | Friction + body modes |
| TopologyFilter (8 nodes) | ~320 | 0.5% | Same as lattice but per-sample input |
| FMCluster (6 ops) | ~192 | 0.3% | sinf → fmodf+sinf (could use quadrature) |
| CoupledOscillators (8) | ~128 | 0.2% | N² coupling forces |
| CohomologyVerb (8 delays) | ~120 | 0.2% | 8×8 matrix mul |
| FibonacciComb (14 taps) | ~100 | 0.1% | 14 delay reads + lowpass |
| TopologyDelay | ~80 | 0.1% | 1 delay + 4 node scatter |
| CohomologySynth (12 modes) | ~72 | 0.1% | Phase quadrature (optimized) |

*Budget: 48kHz, 256-sample buffer, single core @ 3.2 GHz = ~17M cycles/buffer

**All 11 modules running simultaneously: ~3800 cycles/sample × 256 = ~973K cycles = 5.7% of a single core.**

This is very comfortable. Even the topology_advanced prefab (which has 8 of our modules plus standard BespokeSynth modules) would use well under 15% of a core.

### Where compute spikes happen

1. **MembraneSynth at grid size 16**: 256 cells per sample is the heaviest operation. At grid size 8 (64 cells), it's 4× cheaper.

2. **CohomologySynth topology change**: Jacobi eigendecomposition is O(N³) but N≤12 so it takes ~microseconds. The one-time cost is negligible.

3. **CohomologyVerb preset change**: BuildFDN() clears 8 × 48000 floats = 1.5MB of memset. This takes ~0.4ms — audible as a tiny click if done during playback. Should defer to next audio buffer boundary.

4. **FMCluster with 6 ops**: each sample does 6 sinf() calls (via fmodf+sinf). sinf costs ~20 cycles. Could be replaced with quadrature like CohomologySynth to save ~100 cycles/sample. Not urgent but would help at 48kHz on slower hardware.

5. **BowedString friction computation**: the hyperbolic friction function uses 1 division + 1 fabsf per sample per string. Division is ~15 cycles on M-series. With 4 strings, that's 60 cycles of division. Acceptable.

### Visualization cost

The NanoVG rendering is harder to quantify. The most expensive visualizations:

1. **MembraneSynth heatmap**: iterates ~6400 pixels (240×240 at 3px step), each with nvgBeginPath + nvgRect + nvgFill. At 60fps, that's ~384K NanoVG calls/second. This is the most GPU-intensive viz.

2. **SeaOfGrain particles**: up to 32 grains × nvgRadialGradient each. ~192 NanoVG gradient fills per frame.

3. **Vocoder spectral display**: 3 plots × 128 bins × (nvgLineTo + stroke) = ~768 path points per frame.

All of these are CPU-rendered via NanoVG. On M-series, NanoVG comfortably handles ~10K draw calls at 60fps. We're well within budget.

### What would break

**The scenario that breaks things:** 20 MembraneSynth instances at grid size 16, all processing simultaneously. That's 20 × 1536 = 30720 cycles/sample × 256 = 7.9M cycles/buffer = ~46% of a single core. Add the rest of BespokeSynth's processing and you'd start dropping buffers.

**The fix if needed:** MembraneSynth should offer a "lite" mode at half grid resolution (8×8 = 64 cells instead of 256). Same musical result, 4× cheaper. Or use SIMD for the FDTD stencil — the 4-neighbor sum is perfectly suited to SSE/NEON.

---

## Verdict

Our modules are lightweight. The entire topology suite uses less RAM than a single VST plugin and less CPU than a single polyphonic wavetable synth. The hardware can easily weather everything we've built, even running all modules simultaneously.

The only module that warrants attention is MembraneSynth at full grid resolution, and only if many instances run at once. Everything else is trivially cheap.
