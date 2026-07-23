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
- `ricoh_gr_sticks3_selfie_hotshoe_adapter.stl` - upright selfie-screen adapter
- `ricoh_gr_sticks3_selfie_hotshoe_adapter.step` - editable upright adapter
- `generate_adapter.py` - reproducible parametric generator

Running the generator also creates an editable FreeCAD document and a small
camera-shoe fit-coupon STL in this directory.

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
-Y on the lens-facing side while keeping most of its body above the shoe. The
two-hole line is mirrored across the StickS3's long-axis center, placing it
39.5 mm from the model's X origin instead of 8.5 mm. Its screw passages and
90-degree countersinks are circular because they print vertically in the
recommended orientation.

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
