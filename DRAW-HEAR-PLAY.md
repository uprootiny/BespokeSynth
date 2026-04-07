# Draw a Graph, Hear the Timbre — A Design System

Not a module. A principle. A way of making instruments that puts the musician's intuition first.

---

## The Problem with Our Modules Right Now

We built 13 modules. Each one has a visualization that shows the DSP state and a set of slider controls that tweak parameters. The visualization and the controls are separate. You look at the graph, then you look at the sliders, then you listen. Three separate acts.

A great instrument collapses these into one: **you touch the thing that makes the sound, and the thing you touch IS the sound.** A guitar string. A drum head. A piano key. The interface, the physics, and the visualization are the same object.

Our modules have the physics and the visualization. What they lack is the TOUCH. You can't click on a lattice node to excite it. You can't drag a membrane to change the strike position. You can't draw a modulation edge in FMCluster by dragging between nodes. Everything goes through sliders, which are the flattest possible interface.

## The Principle: Direct Manipulation of the Topology

Every topology visualization should be a control surface. The rule:

1. **If you can see it, you can click it.** Lattice nodes are clickable excitation points. Membrane cells are strikeable. FM nodes are draggable.
2. **If you can see a connection, you can change it.** Drag between FM nodes to create/strengthen modulation edges. Drag the coupling strength between exciter and resonator.
3. **If you can see a parameter's effect, the parameter lives WHERE the effect is visible.** Don't put "bow position" in a slider — put it as a draggable marker ON the string.
4. **The visualization responds to touch before the sound does.** Visual feedback is instant (next frame). Audio feedback follows (next buffer). This makes the instrument feel alive even at high latencies.

## What This Looks Like for Each Module

### LatticeSynth
**Now:** Numeric slider for excite node, reflection, topology dropdown.
**Better:**
- Click any node to set it as the excitation point (replaces slider)
- Drag between two nodes to adjust the reflection coefficient at that junction
- Click the space between nodes to add/remove nodes
- The topology selector becomes a gesture: drag the last node onto the first to create a ring. Drag it past the first to create Möbius (visual twist appears).
- Right-click a node to set corruption type (context menu)

### MembraneSynth
**Now:** Sliders for strike X/Y and pickup X/Y.
**Better:**
- Click anywhere on the membrane heatmap to strike at that point
- The strike is instant — note velocity comes from click velocity (distance mouse moved during click)
- Drag on the membrane to set the pickup position (shown as ear icon)
- Pinch (scroll) on the membrane to adjust tension
- The boundary type toggle becomes a visual: drag the edge of the membrane to "clamp" or "free" it

### FMCluster
**Now:** Fixed modulation matrix from code defaults. Ratio and level sliders.
**Better:**
- Drag from one node to another to create a modulation connection
- The drag distance becomes the modulation depth
- Drag an edge to adjust its depth (thicker = more modulation)
- Right-click an edge to delete it
- Drag a node outward/inward to adjust its frequency ratio (farther = higher ratio)
- Double-click a node to toggle carrier/modulator status
- The visualization IS the instrument's algorithm — what you see is what you patch

### BowedString
**Now:** Slider for bow position, velocity, pressure.
**Better:**
- Drag vertically on the string to bow it (Y velocity = bow velocity)
- Drag horizontally to move the bow position
- Click pressure: harder click = more pressure (trackpad force if available)
- The bow contact glow follows your finger/mouse in real-time
- Release to stop bowing (natural note-off)

### CoupledOscillators
**Now:** Slider for coupling, spread.
**Better:**
- Click any mass to pluck it (excite that specific mass)
- Drag between masses to adjust the spring coupling between them (visible as spring thickness)
- Drag a mass left/right to detune it (change its frequency ratio)
- Multiple masses can be plucked simultaneously (multi-touch or chord input)

### TopologySynth
**Now:** Three panel sections with sliders.
**Better:**
- The lattice visualization in the center is the primary control surface
- Click a node = set excite point
- Drag the topology shape (circle for ring, line for chain)
- The stage panels (EXCITE, LATTICE, SHAPER) slide out on hover and collapse when not in use
- The scope in the corner is always visible and expandable on click

### CohomologyVerb
**Now:** Dropdown for preset topology.
**Better:**
- Click to add vertices to the complex
- Drag between vertices to add edges
- Click inside a triangle to add a face (if all three edges exist)
- The reverb character changes as you build the complex
- The Betti numbers update in real-time as you draw
- You're literally constructing the space you want your sound to reverberate in

## The Jam Space

Beyond individual modules — what would make the whole CANVAS better for jamming?

### Quick-Sketch Patches
- **Double-click on empty canvas → spawn a QuickSketch module**: a combined note source + synth + output in one module. Instantly makes sound. Expand into full modules later.
- **Drag from one module's output to empty canvas → auto-spawn a compatible module**: if you drag from an audio output, it spawns a gain or effect. From a note output, it spawns a synth.

### Performance Mode
- **Press P** to enter performance mode: all module chrome (title bars, borders, buttons) fades away. Only visualizations and large controls remain. The canvas becomes a performance surface.
- **Each module's visualization fills its entire space** — no wasted pixels.
- **Touch targets are enlarged** — everything is finger-friendly for touchscreen or trackpad use.

### Sound Snapshots with Interpolation
- **Shift+click anywhere on canvas** → save current state of ALL modules as a snapshot
- **Snapshots appear as small dots on the canvas** — click to recall
- **Drag between two snapshot dots** to crossfade between their states
- The crossfade is per-parameter, weighted by distance — smooth timbral morphing

### Modulation by Gesture
- **Hold Alt + drag any control** → record the movement as a looping modulation curve
- The curve is visible as a thin line overlaid on the control
- It loops at the transport tempo
- This turns every parameter into a potential LFO target without patching

### Audio Scrapbook
- **Drop any audio file onto the canvas** → creates a sample buffer
- **Drag a selection from the buffer onto a MembraneSynth** → uses it as excitation
- **Drag onto a Looper** → loads into the loop buffer
- **Drag onto a FibonacciComb** → processes through the filter and creates a new buffer
- Audio material flows freely between modules via drag-and-drop

## The Graph Editor (standalone module)

A dedicated **GraphEditor** module that unifies the "draw a graph" principle:

- Empty canvas where you draw vertices, edges, and faces
- Each graph element has a type assignment: vertex = oscillator, edge = waveguide, face = cavity
- The graph IS the instrument — it synthesizes sound based on its topology
- You can save graph shapes as presets
- You can morph between graph shapes over time (topology interpolation)
- The graph can be exported to feed any other module: LatticeSynth reads the graph's edges, CohomologySynth reads its simplicial structure, FMCluster reads it as a modulation matrix

This is the capstone: one control surface that speaks to all the topology-based synthesis engines.

## What to Build First

The highest-impact changes, ranked by how much they'd change the FEEL of using the modules:

1. **FMCluster: drag-to-patch edges between nodes** — this alone makes FMCluster feel like a real instrument designer instead of a parameter tweaker. Estimated: 2 hours to add mouse interaction to the graph viz.

2. **MembraneSynth: click-to-strike on the heatmap** — instant gratification. Click the drum, hear the drum. Strike position = where you clicked. Estimated: 1 hour.

3. **LatticeSynth: click-to-excite nodes** — same principle. Click a node, it rings. Estimated: 1 hour.

4. **Performance mode (all modules)** — a global toggle that strips chrome and enlarges visualizations. Estimated: 4 hours (affects IDrawableModule base class).

5. **BowedString: drag-to-bow** — the most physically intuitive. Drag on the string = bowing. Estimated: 2 hours.

Everything else is valuable but these five changes would transform the suite from "interesting modules with nice visualizations" to "instruments you play by touching."
