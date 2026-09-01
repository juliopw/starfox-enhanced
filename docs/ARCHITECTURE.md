# Port architecture

## Why presentation FPS cannot drive gameplay

UltraStarFox's `transfer_l` routine runs `dostrats` once for a completed game
frame, and `dostrats` increments `gameframe` before updating the object
strategies. The original renderer/transfer pipeline is spread across three IRQ
phases. Running the entire loop 60 times per second therefore makes movement,
map waits, attacks, damage, RNG consumption, and animation three times too
fast.

The native loop separates those responsibilities:

```text
sample real time and input
        |
        v
fixed 20 Hz simulation ----> previous/current fixed-point snapshots
        |                                      |
        |                                      v
        +----------------------------> interpolate(alpha) at
                                      selected render FPS
                                               |
                                               v
                                 present at 20/30/60/90/
                                  120/240/360/480 FPS
```

Input edges are latched until a simulation tick consumes them. Holding a key
is represented in every tick; a press and release between two ticks is not
lost. Pausing stops the simulation accumulator while UI presentation can keep
running.

The timing code converts each selectable output rate through an exact 1440 Hz
integer timebase into the fixed 60 Hz cartridge raster. That raster continues
to drive the deterministic 20 Hz simulation, frontend sequencing, and audio.
Floating-point values exist only in the rendered transform, normalized matrix
interpolation, and fractional interpolation alpha; the simulation remains
fixed-point and deterministic.

## Model pipeline

The readable shape sources are excellent for inventory and diagnostics, but
they are not the final authority: assembler conditionals, symbol expressions,
custom point macros, and wireframe macros alter the emitted bytes. The exact
asset pipeline is therefore:

1. Build the pinned UltraStarFox configuration.
2. Read symbol addresses from its generated map/symbol report.
3. Decode the assembled shape headers and command streams into a lossless
   intermediate representation.
4. Cache that representation locally with the source revision and build
   configuration hash.
5. Feed the same representation to the reference CPU renderer and the eventual
   GPU renderer.

The intermediate representation preserves:

- signed 8-bit and 16-bit coordinates and each header's `sh_shift`;
- animated `Frames`/`JumpTab` point sets;
- 2- through 12-vertex faces and wireframe edge expansion;
- visibility normals, BSP traversal, face order, and line semantics;
- color-table indexes, animated colors, lighting modes, and texture IDs;
- shadow and three LOD references;
- collision bounds and animated collision frames.

Converting models through OBJ or glTF is not the canonical path because those
formats lose several of these game-specific semantics. Exporters can be added
for inspection only.

## Rendering boundary

The runtime renderer is a C++ translation of the Super FX shape command
interpreter and rasterizer. It renders the original 224x192 viewport, or a
horizontally extended 360x224, 400x224, 520x224, or 800x224 scene, into an
indexed-color
framebuffer. It preserves source face order, fixed-point matrices, near/screen
clipping, polygon and texture scan conversion, line rules, sprite-faces,
shadows, LOD selection, lighting, and depth palettes.

Mode 1/2/3 background layers, OAM priorities, HUD meters, route-map spheres,
and custom screen graphics are composed separately from 3D. This keeps PPU
priority semantics out of the model decoder and allows the selectable
presentation loop to interpolate only world/camera transforms.

2D backgrounds, HUD, sprite priority, color math, and window effects are a
separate PPU-facing layer. They must not be folded into the 3D model renderer.

## Graphics-API boundary

The whole graphics-API surface sits behind one interface in `starfox::gfx`, so
adding a technology is one translation unit and one registry entry rather than
a rewrite. Nothing in `render_backend.hpp` names a graphics API, a windowing
library, or a platform.

The line is drawn where the cartridge stops deciding. Projection, face
visibility, near and screen clipping, and draw order all run in the source's
own fixed-point arithmetic above the boundary; a backend receives faces that
are already clipped and already ordered, in source raster units with the
fraction kept, and rasterizes and composites them. Submission order is the
contract, because that is how the source decides what covers what: there is no
depth buffer and no sorting below the line.

Two things travel with those faces, and both are requirements rather than
extras:

- The frame comes back as palette indices. The port composites its layers,
  fades them, and post-processes them in index space, so a backend that can
  only return color cannot stand in for the built-in fill.
- Filling a face also records that face's camera-space normal and mean depth
  for every pixel it covers. `ENHANCED GRAPHICS`, `SMOOTH POLYS`, and
  `RTX LIGHTING` read that record instead of guessing surface orientation from
  palette brightness. It is the largest per-frame allocation at a high render
  scale, so it is only produced when one of those three is on. Lines and
  cartridge art cover pixels without claiming a surface, in both the built-in
  fill and every backend.

`Capabilities` states whether a backend can do each of those; the runtime
refuses one that cannot and falls back to software with a diagnostic, rather
than presenting a blank screen or three silently dead options. `RenderOptions`
is a request a backend may clamp and report back through `applied_options()`,
which is how the render scale and the surface record are negotiated.

The reference software backend needs no device, so it runs in the tests and on
a build machine and is the yardstick a device backend is measured against.

## Logic-port boundary

Port subsystems in this order:

1. memory types, fixed-point math, matrices, and RNG;
2. object list and strategy scheduler;
3. PATH and map command interpreters;
4. player movement, weapons, collision, and damage;
5. stage-specific strategies and bosses;
6. menus, HUD, 2D effects, audio commands, and save data.

The strategy and map interpreters should initially retain the original opcode
data. Rewriting every stage as new C++ behavior would make timing equivalence
much harder to prove.

### Native compatibility bridge

The port has a project-owned SNES memory adapter around a
pinned 65C816 core. It maps LoROM, WRAM, WRAM mirrors, and the joypad register,
then synchronizes the original 56-byte `alblks` records, parallel `xalblks`
records, and active/free lists with typed C++ objects. This lets inline map
blocks, map `JSL` controls, map predicates, `INITGAME_STRATS_L`, and
`DO_STRAT_L` execute without approximating their side effects.

Unimplemented hardware registers retain open-bus behavior; they must only be
mapped when their semantics are verified. A blanket zero-filled I/O map is
incorrect and can change control flow. The bridge is a deterministic
compatibility/oracle layer, not permission for presentation timing or host
floating point to enter gameplay state.

## Equivalence testing

A deterministic controller-input trace is played in both the UltraStarFox
reference build and the native port. At every 20 Hz logic boundary, compare:

- `gameframe`, stage/map cursor, RNG state, score, health, and inventory;
- every live object's type, strategy state, flags, transform, velocity,
  animation frame, collision frame, and health;
- spawned/removed object order and audio command stream.

Render validation uses golden 224x192 indexed framebuffers at logic boundaries.
Interpolated frames between boundaries are validated for endpoint identity and
monotonic transform motion; they are new presentation frames and cannot have
an original framebuffer equivalent.

The automated suite currently covers every assembled shape header, all 40
shipped Star Fox EX stage labels from `PLANETS` and `PLANETS2` under 2,000-tick
passive execution, Original and EX route transitions,
title/training/planet/game-over/continue/credits flow, source PPU assets,
SPC700 output, and deterministic timing primitives. The unused EX `PLANETS3`
test campaign is deliberately excluded. Reference-controller traces remain
the preferred way to investigate any future first-divergence report; visual
similarity alone is never used as a logic oracle.
