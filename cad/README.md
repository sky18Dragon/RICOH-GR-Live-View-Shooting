# StickS3 hot-shoe adapter

This folder contains a bolt-on adapter that uses the two existing M2 threaded
holes on the back of the M5Stack StickS3 and slides into the RICOH GR hot shoe.
The StickS3 is oriented landscape. The shoe foot is aligned to the adapter's
Y-max side so the part has a shared flat edge for support-light printing. The
plate corners are chamfered, and the horizontal screw passages are teardrops
whose points face away from the aligned print edge.

## Files

- `ricoh_gr_sticks3_hotshoe_adapter.stl` - printable adapter
- `ricoh_gr_sticks3_hotshoe_adapter.step` - editable neutral CAD solid
- `ricoh_gr_sticks3_selfie_hotshoe_adapter_holes_x8_5.*` - upright adapter
  with the M2 hole line at X=8.5 mm (`.stl` and `.step`)
- `ricoh_gr_sticks3_selfie_hotshoe_adapter_holes_x39_5.*` - mirrored upright
  adapter with the M2 hole line at X=39.5 mm (`.stl` and `.step`)
- `ricoh_gr_sticks3_hinged_selfie_hotshoe_base.*` - camera-side hinge half
  with the hot-shoe foot (`.stl` and `.step`)
- `ricoh_gr_sticks3_hinged_selfie_screen_plate.*` - StickS3 mounting plate
  with both mirrored M2 patterns (`.stl` and `.step`)
- `ricoh_gr_sticks3_hinged_selfie_adapter.FCStd` - editable FreeCAD assembly
  with upright/folded references and the filament pin reference
- `generate_adapter.py` - reproducible parametric generator

Running the generator also creates an editable FreeCAD document for each
orientation and a small camera-shoe fit-coupon STL in this directory.

## Verified source dimensions

- StickS3 body: 48 x 24 x 15 mm
- StickS3 fasteners: 2 x M2, 18 mm centre-to-centre
- Fastener line: 8.5 mm from the nearby short edge
- ISO 518 shoe: 18.6 mm minimum internal width, 12.5 mm minimum throat,
  and 2.0 mm nominal slot height

The printed foot is 18.0 x 15.4 x 1.95 mm, with an 11.8 mm neck and 1.45 mm
rail gap. This adds 0.30 mm of vertical preload while keeping the mounting
plate and screw-head clearance at the same height above the camera.

## Upright selfie-screen variant

The selfie variant keeps the same ISO 518 foot, 0.30 mm preload, and two M2
anchor locations. A compact reinforcing pad connects the shoe neck directly to
a 90-degree wall, turning the StickS3 upright in landscape orientation without
the flat adapter's 30 x 24 mm plate. The wall's Y-max face, compact pad, and
shoe foot share one flush edge, providing a large flat print surface. The
StickS3 back mounts against the wall's Y-min face, placing the display toward
-Y on the lens-facing side while keeping most of its body above the shoe.

The generator exports both long-axis orientations:

- `holes_x8_5`: M2 hole line at X=8.5 mm
- `holes_x39_5`: hole line mirrored across the StickS3 center to X=39.5 mm

Choose the version that places the display and controls in the preferred
direction on the camera. Both use circular screw passages and 90-degree
countersinks because the holes print vertically in the recommended orientation.

## Two-piece hinged selfie-screen variant

The hinged version replaces the two orientation-specific rigid adapters with a
single two-piece assembly:

- a compact hot-shoe base with one fixed center knuckle
- a StickS3 plate with two outer knuckles and both X=8.5/X=39.5 M2 pairs

The hinge axis runs along the StickS3's 48 mm dimension at the camera-front
edge of the shoe. The screen rests display-up when folded and rotates 90 degrees
upward to face the front of the camera for selfie framing. The generator checks
the complete rotation for solid-to-solid collisions.

### Filament-pin dimensions

- Pin: nominal 1.75 mm filament, cut to approximately 26.4 mm
- Bore: 2.0 mm
- Bore roof relief: a full teardrop extending approximately 0.414 mm outside
  the complete 2.0 mm circle, pointed away from each part's build plate. Its
  two roof faces meet the circle tangentially at 45 degrees and never reduce
  the nominal round pin passage
- Knuckle outside diameter: 5.5 mm
- Two shortened 6.2 mm moving knuckles and one 11.8 mm fixed center knuckle,
  with 0.3 mm axial gaps over the original 24.8 mm active span
- 2.5 mm internal lead-ins at the knuckle seams
- A deep camera-side support ramp extending almost to the hinge axis
- The fixed center knuckle is widened to the hot-shoe neck's full 11.8 mm and
  aligns flush with both sides of that upper structure for a stronger joint
- The rear support plane is also 11.8 mm wide, matching the center knuckle and
  hot-shoe neck instead of extending beyond the 18 mm shoe foot
- The fixed cylinder's outside bottom sits 2.3 mm above the hot-shoe insert
  bottom, matching the measured camera envelope in front of the shoe and
  reducing the camera-side part's vertical thickness by 1.2 mm
- A full-width lower rear gusset fills the concave divot between the lowered
  cylinder and shoe neck down to that same 2.3 mm clearance plane, giving the
  joint continuous load-bearing material instead of a thin tangent overlap
- The two-knuckle mounting plate and all four screw holes move down 1.15 mm
  with the lowered hinge, returning the outer knuckles flush with the plate's
  lower edge instead of leaving them exposed below it
- The entire hinge is pushed forward until every cylinder's rear tangent sits
  0.2 mm beyond the hot-shoe front edge, clearing the camera's raised retaining
  rails. The hinge now extends 5.70 mm beyond the shoe front, and the folded
  StickS3 plate center sits 1.40 mm forward of the shoe center rather than
  reintroducing any rear overhang
- The camera-facing caps of the two moving outer knuckles are flattened by
  0.50 mm. In the folded position their lowest surface is exactly 2.80 mm above
  the hot-shoe base, providing insertion clearance without moving the hinge
  axis, fixed center knuckle, or filament bore
- The camera-facing lower half of the fixed barrel is filled down to the shoe
  structure, removing the side-view undercut and supporting flat-bed printing
- Its upper camera-facing quadrant is filled as well, squaring the fixed
  knuckle's top-left side profile
- The hinge-facing hot-shoe corners remain rounded, while the two corners on
  the edge farthest from the hinge return to hard 45-degree bevels for
  edge-down printing. The same hybrid profile continues through the upper
  support and fixed-knuckle structure
- One short sub-45-degree ramp joins that plane to the fixed center knuckle
  while remaining clear of the moving knuckles and the folded screen plate
- The hinge uses one fixed center barrel and two moving outer barrels; the
  former outer fixed barrels and the temporary stop nubs have been removed
- The mounting plate surrounds the outer barrels and has a square-ended,
  edge-open center slot around the fixed barrel. This removes the fragile thin
  web, leaves every pin bore unobstructed, and forms the folded 90-degree stop.
- The mounting plate is trimmed to 37 mm and uses true 3 mm corner radii. Each
  corner arc is centered on its countersunk screw hole, leaving a consistent
  1 mm radial border around the 4 mm countersink at the outside corners.
- Two straight 2.0 mm feed channels extend the hinge bore into the inner side
  of the lower countersinks. The screw recess exposes each loading point while
  the outer half of the countersink and the plate edge remain intact.
- A shallow stop pad recessed inside the square center-relief roof bears on the
  fixed knuckle just beyond 90 degrees. The rest of the relief retains 0.4 mm
  running clearance, and the hot-shoe half has no protruding stop bump

The 0.3 mm knuckle gaps use a close common starting clearance for moving FDM
parts. The 2.0 mm bore intentionally gives a light-friction fit around
nominal 1.75 mm filament; actual fit still depends on printer calibration,
material shrinkage, layer height, and the measured filament diameter.

### Print and assemble

1. Print each STL separately at 0.15-0.20 mm layers with at least four
   perimeters. The base STL is laid on the beveled edge farthest from the
   hinge, and the screen-plate STL is laid flat on its broad Y-min face. No
   manual rotation should be necessary. STEP files retain the upright assembly
   coordinates for editing.
2. Attach the StickS3 to the screen plate with two M2x4 flat-head screws. Use
   only the X=8.5 or X=39.5 pair matching the desired device direction.
3. Place the fixed center knuckle between the plate's two outer knuckles.
4. Cut about 26.4 mm of 1.75 mm PETG, PA, or PLA filament. Bevel one end,
   place it in either lower countersink's straight loading guide, and push it
   through the aligned hinge bore without forcing it.
5. If the bore is tight, clear it by hand with a 2.0 mm drill bit or pin reamer;
   do not use a powered drill near the small knuckles.
6. Trim the pin to about 0.8 mm proud at each end. Lightly mushroom the ends
   against the outer barrel faces with a temperature-controlled soldering-iron
   tip. Apply only enough heat to retain the pin without melting the hinge
   barrels.

## Hardware and printing

- 2 x M2x4, 90-degree flat-head screws
- PETG, ASA, ABS, or PA recommended; avoid brittle silk PLA
- 0.15-0.20 mm layers, 4 perimeters, 40-60% infill
- Print the adapter standing on the side where the mounting plate and shoe foot
  are flush. Add a brim for stability; only limited support should be needed.
- Print the upright selfie variant on the shared Y-max edge where its wall,
  compact pad, and hot-shoe foot are flush. The screw passages are vertical in
  this orientation, and the part should need no internal support. Add a brim if
  the wall needs more bed adhesion.

For a fit test, run the generator and print its fit coupon first. It should
slide in without force and remain removable by its pull tab. Never force a
tight print into the camera. If the coupon is tight, reduce
`SHOE_FOOT_WIDTH` and/or `SHOE_FOOT_THICKNESS` in `generate_adapter.py` by
0.1-0.2 mm and regenerate.

Before installing the adapter, verify the available thread depth with the
original hardware. Start each screw by hand and stop immediately if it bottoms
out. Do not use a screw length that can contact internal StickS3 components.

## Regenerate on macOS

```bash
/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd \
  cad/generate_adapter.py
```
