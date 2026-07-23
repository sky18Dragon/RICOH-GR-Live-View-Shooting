"""Generate the StickS3-to-RICOH-GR hot-shoe adapter artifacts.

Run inside FreeCAD, for example on macOS:
  /Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd cad/generate_adapter.py

All dimensions are millimetres. The camera-facing geometry is deliberately
undersized relative to the ISO 518 minimum shoe envelope for FDM clearance.
"""

from pathlib import Path
import math

import FreeCAD as App
import Mesh
import Part
from FreeCAD import Vector


OUTPUT_DIR = Path(__file__).resolve().parent
DOCUMENT_NAME = "RicohGR_StickS3_Hotshoe_Adapter"
SELFIE_DOCUMENT_NAME = "RicohGR_StickS3_Selfie_Hotshoe_Adapter"

# M5Stack K150 StickS3 mechanical drawing.
STICK_WIDTH = 24.0
STICK_LENGTH = 48.0
SCREW_DIAMETER = 2.2
SCREW_SPACING = 18.0
SCREW_FROM_SHORT_EDGE = 8.5

# Adapter backing plate.
PLATE_X_START = 4.0
PLATE_Y_START = 0.0
PLATE_LENGTH = 30.0
PLATE_WIDTH = 24.0
PLATE_THICKNESS = 2.4

# Conservative ISO 518 male-foot dimensions.
SHOE_FOOT_WIDTH = 18.0
SHOE_FOOT_LENGTH = 15.4
SHOE_FOOT_THICKNESS = 1.95
SHOE_NECK_WIDTH = 11.8
SHOE_NECK_HEIGHT = 1.45

# Upright selfie-screen bracket. The wall's Y-max face is flush with the shoe
# foot's Y-max edge, creating one shared print plane. Its Y-min face receives
# the back of the StickS3, so the display faces -Y while remaining landscape.
SELFIE_WALL_THICKNESS = 2.4
SELFIE_WALL_LENGTH = STICK_LENGTH
SELFIE_MOUNT_PAD_WIDTH = SHOE_FOOT_WIDTH
SELFIE_MOUNT_PAD_DEPTH = 8.0
SELFIE_SCREW_X_NEAR = SCREW_FROM_SHORT_EDGE
SELFIE_SCREW_X_MIRRORED = STICK_LENGTH - SCREW_FROM_SHORT_EDGE
SELFIE_VARIANTS = (
    ("holes_x8_5", SELFIE_SCREW_X_NEAR, 4.0),
    (
        "holes_x39_5",
        SELFIE_SCREW_X_MIRRORED,
        STICK_LENGTH - 4.0 - 27.0,
    ),
)


def chamfered_box(x0, y0, z0, length, width, height, chamfer):
    x1, y1 = x0 + length, y0 + width
    points = [
        Vector(x0 + chamfer, y0, z0),
        Vector(x1 - chamfer, y0, z0),
        Vector(x1, y0 + chamfer, z0),
        Vector(x1, y1 - chamfer, z0),
        Vector(x1 - chamfer, y1, z0),
        Vector(x0 + chamfer, y1, z0),
        Vector(x0, y1 - chamfer, z0),
        Vector(x0, y0 + chamfer, z0),
    ]
    return Part.Face(Part.makePolygon(points + [points[0]])).extrude(
        Vector(0, 0, height)
    )


def teardrop_wire(cx, cy, z0, radius, point_toward_negative_y=True, segments=24):
    """Return a horizontal-hole profile with a 45-degree printable roof."""
    direction = -1.0 if point_toward_negative_y else 1.0
    if direction < 0:
        start_angle, end_angle = -45.0, 225.0
    else:
        start_angle, end_angle = 135.0, 405.0

    arc_points = []
    for index in range(segments + 1):
        angle = math.radians(
            start_angle + (end_angle - start_angle) * index / segments
        )
        arc_points.append(
            Vector(cx + radius * math.cos(angle), cy + radius * math.sin(angle), z0)
        )

    apex = Vector(cx, cy + direction * math.sqrt(2.0) * radius, z0)
    points = [apex] + arc_points
    return Part.makePolygon(points + [points[0]])


def teardrop_prism(cx, cy, z0, radius, height):
    return Part.Face(teardrop_wire(cx, cy, z0, radius)).extrude(
        Vector(0, 0, height)
    )


def chamfered_foot(cx, cy, z0, width, length, height, chamfer):
    x0, x1 = cx - width / 2.0, cx + width / 2.0
    y0, y1 = cy - length / 2.0, cy + length / 2.0
    points = [
        Vector(x0 + chamfer, y0, z0),
        Vector(x1 - chamfer, y0, z0),
        Vector(x1, y0 + chamfer, z0),
        Vector(x1, y1 - chamfer, z0),
        Vector(x1 - chamfer, y1, z0),
        Vector(x0 + chamfer, y1, z0),
        Vector(x0, y1 - chamfer, z0),
        Vector(x0, y0 + chamfer, z0),
    ]
    wire = Part.makePolygon(points + [points[0]])
    return Part.Face(wire).extrude(Vector(0, 0, height))


def chamfered_wall(x0, y0, z0, length, depth, height, chamfer):
    x1, z1 = x0 + length, z0 + height
    points = [
        Vector(x0 + chamfer, y0, z0),
        Vector(x1 - chamfer, y0, z0),
        Vector(x1, y0, z0 + chamfer),
        Vector(x1, y0, z1 - chamfer),
        Vector(x1 - chamfer, y0, z1),
        Vector(x0 + chamfer, y0, z1),
        Vector(x0, y0, z1 - chamfer),
        Vector(x0, y0, z0 + chamfer),
    ]
    return Part.Face(Part.makePolygon(points + [points[0]])).extrude(
        Vector(0, depth, 0)
    )


def make_adapter():
    # Screen is landscape on the camera: StickS3 long axis is X.
    stick_cx = STICK_LENGTH / 2.0
    # Align the foot to the Y-max plate edge, creating one shared flat plane
    # so the adapter can be printed standing on that edge.
    shoe_cy = PLATE_Y_START + PLATE_WIDTH - SHOE_FOOT_LENGTH / 2.0
    screw_x = SCREW_FROM_SHORT_EDGE
    screw_ys = (
        (STICK_WIDTH - SCREW_SPACING) / 2.0,
        (STICK_WIDTH + SCREW_SPACING) / 2.0,
    )

    plate_z = SHOE_FOOT_THICKNESS + SHOE_NECK_HEIGHT
    foot = chamfered_foot(
        stick_cx,
        shoe_cy,
        0.0,
        SHOE_FOOT_WIDTH,
        SHOE_FOOT_LENGTH,
        SHOE_FOOT_THICKNESS,
        1.0,
    )
    neck = chamfered_foot(
        stick_cx,
        shoe_cy,
        SHOE_FOOT_THICKNESS - 0.05,
        SHOE_NECK_WIDTH,
        SHOE_FOOT_LENGTH,
        SHOE_NECK_HEIGHT + 0.10,
        0.7,
    )
    plate = chamfered_box(
        PLATE_X_START,
        PLATE_Y_START,
        plate_z,
        PLATE_LENGTH,
        PLATE_WIDTH,
        PLATE_THICKNESS,
        2.0,
    )
    adapter = foot.fuse(neck).fuse(plate).removeSplitter()

    # M2 clearance plus a 90-degree flat-head countersink from below.
    for screw_y in screw_ys:
        through = teardrop_prism(
            screw_x,
            screw_y,
            plate_z - 0.3,
            SCREW_DIAMETER / 2.0,
            PLATE_THICKNESS + 0.6,
        )
        countersink = Part.makeLoft(
            [
                teardrop_wire(screw_x, screw_y, plate_z - 0.01, 2.0),
                teardrop_wire(
                    screw_x,
                    screw_y,
                    plate_z + 1.10,
                    SCREW_DIAMETER / 2.0,
                ),
            ],
            True,
            False,
        )
        adapter = adapter.cut(through.fuse(countersink))

    return adapter.removeSplitter()


def make_fit_coupon():
    foot = chamfered_foot(
        0.0,
        0.0,
        0.0,
        SHOE_FOOT_WIDTH,
        SHOE_FOOT_LENGTH,
        SHOE_FOOT_THICKNESS,
        1.0,
    )
    neck = chamfered_foot(
        0.0,
        0.0,
        SHOE_FOOT_THICKNESS - 0.05,
        SHOE_NECK_WIDTH,
        SHOE_FOOT_LENGTH,
        SHOE_NECK_HEIGHT + 0.10,
        0.7,
    )
    plate_z = SHOE_FOOT_THICKNESS + SHOE_NECK_HEIGHT
    pull_tab = chamfered_box(-11.0, 6.7, plate_z, 22.0, 6.0, 2.0, 1.5)
    bridge = Part.makeBox(
        SHOE_NECK_WIDTH,
        2.0,
        2.1,
        Vector(-SHOE_NECK_WIDTH / 2.0, 5.7, plate_z - 0.05),
    )
    return foot.fuse(neck).fuse(pull_tab).fuse(bridge).removeSplitter()


def make_selfie_adapter(screw_x):
    """Create a front-facing upright StickS3 mount on the same shoe geometry."""
    stick_cx = STICK_LENGTH / 2.0
    shoe_cy = PLATE_Y_START + PLATE_WIDTH - SHOE_FOOT_LENGTH / 2.0
    plate_z = SHOE_FOOT_THICKNESS + SHOE_NECK_HEIGHT
    base_top = plate_z + PLATE_THICKNESS
    aligned_y = PLATE_Y_START + PLATE_WIDTH
    wall_y0 = aligned_y - SELFIE_WALL_THICKNESS

    foot = chamfered_foot(
        stick_cx,
        shoe_cy,
        0.0,
        SHOE_FOOT_WIDTH,
        SHOE_FOOT_LENGTH,
        SHOE_FOOT_THICKNESS,
        1.0,
    )
    neck = chamfered_foot(
        stick_cx,
        shoe_cy,
        SHOE_FOOT_THICKNESS - 0.05,
        SHOE_NECK_WIDTH,
        SHOE_FOOT_LENGTH,
        SHOE_NECK_HEIGHT + 0.10,
        0.7,
    )
    mount_pad = chamfered_box(
        stick_cx - SELFIE_MOUNT_PAD_WIDTH / 2.0,
        aligned_y - SELFIE_MOUNT_PAD_DEPTH,
        plate_z,
        SELFIE_MOUNT_PAD_WIDTH,
        SELFIE_MOUNT_PAD_DEPTH,
        PLATE_THICKNESS,
        1.0,
    )
    wall = chamfered_wall(
        0.0,
        wall_y0,
        plate_z,
        SELFIE_WALL_LENGTH,
        SELFIE_WALL_THICKNESS,
        STICK_WIDTH + PLATE_THICKNESS,
        2.0,
    )
    adapter = foot.fuse(neck).fuse(mount_pad).fuse(wall).removeSplitter()

    screw_zs = (
        base_top + (STICK_WIDTH - SCREW_SPACING) / 2.0,
        base_top + (STICK_WIDTH + SCREW_SPACING) / 2.0,
    )
    for screw_z in screw_zs:
        through = Part.makeCylinder(
            SCREW_DIAMETER / 2.0,
            SELFIE_WALL_THICKNESS + 0.6,
            Vector(screw_x, wall_y0 - 0.3, screw_z),
            Vector(0, 1, 0),
        )
        countersink = Part.makeCone(
            2.0,
            SCREW_DIAMETER / 2.0,
            1.11,
            Vector(
                screw_x,
                wall_y0 + SELFIE_WALL_THICKNESS + 0.01,
                screw_z,
            ),
            Vector(0, -1, 0),
        )
        adapter = adapter.cut(through.fuse(countersink))

    return adapter.removeSplitter()


def add_parameters(doc):
    obj = doc.addObject("App::FeaturePython", "Parameters")
    obj.Label = "Adapter Parameters (mm)"
    values = {
        "StickWidth": STICK_WIDTH,
        "StickLength": STICK_LENGTH,
        "ScrewDiameter": SCREW_DIAMETER,
        "ScrewSpacing": SCREW_SPACING,
        "ScrewFromShortEdge": SCREW_FROM_SHORT_EDGE,
        "PlateXStart": PLATE_X_START,
        "PlateYStart": PLATE_Y_START,
        "PlateLength": PLATE_LENGTH,
        "PlateWidth": PLATE_WIDTH,
        "PlateThickness": PLATE_THICKNESS,
        "ShoeFootWidth": SHOE_FOOT_WIDTH,
        "ShoeFootLength": SHOE_FOOT_LENGTH,
        "ShoeFootThickness": SHOE_FOOT_THICKNESS,
        "ShoeNeckWidth": SHOE_NECK_WIDTH,
        "ShoeNeckHeight": SHOE_NECK_HEIGHT,
    }
    for name, value in values.items():
        obj.addProperty("App::PropertyLength", name, "Dimensions")
        setattr(obj, name, value)
    return obj


def add_selfie_parameters(doc, screw_x):
    obj = add_parameters(doc)
    values = {
        "SelfieWallThickness": SELFIE_WALL_THICKNESS,
        "SelfieWallLength": SELFIE_WALL_LENGTH,
        "SelfieMountPadWidth": SELFIE_MOUNT_PAD_WIDTH,
        "SelfieMountPadDepth": SELFIE_MOUNT_PAD_DEPTH,
        "SelfieScrewX": screw_x,
    }
    for name, value in values.items():
        obj.addProperty("App::PropertyLength", name, "Selfie Adapter")
        setattr(obj, name, value)
    obj.addProperty("App::PropertyString", "DisplayFaces", "Selfie Adapter")
    obj.DisplayFaces = "-Y (lens-facing)"


def export_selfie_variant(file_suffix, screw_x, screen_x):
    selfie_doc = App.newDocument(f"{SELFIE_DOCUMENT_NAME}_{file_suffix}")
    add_selfie_parameters(selfie_doc, screw_x)
    selfie_shape = make_selfie_adapter(screw_x)
    if not selfie_shape.isValid() or len(selfie_shape.Solids) != 1:
        raise RuntimeError(
            f"Selfie adapter {file_suffix} did not produce one valid solid"
        )

    selfie = selfie_doc.addObject("Part::Feature", "SelfieAdapter")
    selfie.Label = (
        f"StickS3 Upright Selfie Screen Hot Shoe Adapter ({file_suffix})"
    )
    selfie.Shape = selfie_shape
    if selfie.ViewObject:
        selfie.ViewObject.ShapeColor = (0.30, 0.66, 0.88)
        selfie.ViewObject.LineColor = (0.05, 0.10, 0.18)
    selfie.addProperty("App::PropertyString", "SourceDimensions", "Documentation")
    selfie.SourceDimensions = (
        "Same M5Stack K150 and ISO 518 dimensions as the flat adapter; "
        f"mounting-hole line at X={screw_x:.1f} mm; display faces -Y"
    )
    selfie.addProperty("App::PropertyString", "RecommendedScrews", "Documentation")
    selfie.RecommendedScrews = (
        "2x M2x4 90-degree flat-head; verify thread depth and tighten gently"
    )

    # Reference-only envelope: the StickS3 back sits on the wall's Y-min face,
    # and its display faces -Y. These objects are hidden and never exported.
    base_top = SHOE_FOOT_THICKNESS + SHOE_NECK_HEIGHT + PLATE_THICKNESS
    wall_y0 = PLATE_Y_START + PLATE_WIDTH - SELFIE_WALL_THICKNESS
    envelope = selfie_doc.addObject("Part::Feature", "StickS3Envelope")
    envelope.Label = "StickS3 48 x 24 x 15 mm reference (display faces -Y)"
    envelope.Shape = Part.makeBox(
        STICK_LENGTH,
        15.0,
        STICK_WIDTH,
        Vector(0.0, wall_y0 - 15.0, base_top),
    )
    if envelope.ViewObject:
        envelope.ViewObject.ShapeColor = (0.24, 0.26, 0.30)
        envelope.ViewObject.Transparency = 70
        envelope.ViewObject.Visibility = False

    screen = selfie_doc.addObject("Part::Feature", "ScreenReference")
    screen.Label = "Approximate StickS3 display face (-Y)"
    screen.Shape = Part.makeBox(
        27.0,
        0.3,
        15.0,
        Vector(screen_x, wall_y0 - 15.3, base_top + 4.5),
    )
    if screen.ViewObject:
        screen.ViewObject.ShapeColor = (0.10, 0.70, 0.95)
        screen.ViewObject.LineColor = (0.02, 0.20, 0.28)
        screen.ViewObject.Visibility = False

    output_stem = f"ricoh_gr_sticks3_selfie_hotshoe_adapter_{file_suffix}"
    selfie_doc.recompute()
    selfie_doc.saveAs(str(OUTPUT_DIR / f"{output_stem}.FCStd"))
    Part.export([selfie], str(OUTPUT_DIR / f"{output_stem}.step"))
    Mesh.export([selfie], str(OUTPUT_DIR / f"{output_stem}.stl"))

    selfie_mesh = Mesh.Mesh(str(OUTPUT_DIR / f"{output_stem}.stl"))
    if not selfie_mesh.isSolid():
        raise RuntimeError(f"The {file_suffix} selfie STL is not a closed solid")
    print(
        f"Generated selfie adapter {file_suffix}:",
        round(selfie_shape.BoundBox.XLength, 2),
        "x",
        round(selfie_shape.BoundBox.YLength, 2),
        "x",
        round(selfie_shape.BoundBox.ZLength, 2),
        "mm;",
        selfie_mesh.CountFacets,
        "closed-mesh facets",
    )


def main():
    if App.ActiveDocument:
        App.closeDocument(App.ActiveDocument.Name)
    doc = App.newDocument(DOCUMENT_NAME)
    add_parameters(doc)

    adapter_shape = make_adapter()
    if not adapter_shape.isValid() or len(adapter_shape.Solids) != 1:
        raise RuntimeError("Adapter generation did not produce one valid solid")
    adapter = doc.addObject("Part::Feature", "Adapter")
    adapter.Label = "StickS3 to RICOH GR ISO 518 Hot Shoe Adapter"
    adapter.Shape = adapter_shape
    if adapter.ViewObject:
        adapter.ViewObject.ShapeColor = (0.18, 0.42, 0.78)
        adapter.ViewObject.LineColor = (0.05, 0.10, 0.18)
    adapter.addProperty("App::PropertyString", "SourceDimensions", "Documentation")
    adapter.SourceDimensions = (
        "M5Stack K150: M2 x2, 18 mm centres, 8.5 mm from short edge; "
        "ISO 518:2006 shoe envelope"
    )
    adapter.addProperty("App::PropertyString", "RecommendedScrews", "Documentation")
    adapter.RecommendedScrews = (
        "2x M2x4 90-degree flat-head; verify thread depth and tighten gently"
    )

    coupon_shape = make_fit_coupon()
    if not coupon_shape.isValid() or len(coupon_shape.Solids) != 1:
        raise RuntimeError("Coupon generation did not produce one valid solid")
    coupon = doc.addObject("Part::Feature", "HotshoeFitCoupon")
    coupon.Label = "ISO 518 Fit Coupon - print this first"
    coupon.Shape = coupon_shape
    coupon.Placement.Base = Vector(24.0, 34.0, 0.0)
    if coupon.ViewObject:
        coupon.ViewObject.ShapeColor = (0.92, 0.55, 0.12)

    doc.recompute()
    doc.saveAs(str(OUTPUT_DIR / "ricoh_gr_sticks3_hotshoe_adapter.FCStd"))
    Part.export([adapter], str(OUTPUT_DIR / "ricoh_gr_sticks3_hotshoe_adapter.step"))
    Mesh.export([adapter], str(OUTPUT_DIR / "ricoh_gr_sticks3_hotshoe_adapter.stl"))

    coupon.Placement = App.Placement()
    doc.recompute()
    Mesh.export([coupon], str(OUTPUT_DIR / "ricoh_gr_hotshoe_fit_coupon.stl"))

    adapter_mesh = Mesh.Mesh(str(OUTPUT_DIR / "ricoh_gr_sticks3_hotshoe_adapter.stl"))
    coupon_mesh = Mesh.Mesh(str(OUTPUT_DIR / "ricoh_gr_hotshoe_fit_coupon.stl"))
    if not adapter_mesh.isSolid() or not coupon_mesh.isSolid():
        raise RuntimeError("An exported STL is not a closed solid")
    print(
        "Generated adapter:",
        round(adapter_shape.BoundBox.XLength, 2),
        "x",
        round(adapter_shape.BoundBox.YLength, 2),
        "x",
        round(adapter_shape.BoundBox.ZLength, 2),
        "mm",
    )
    print(
        "Validated closed STL meshes:",
        adapter_mesh.CountFacets,
        "adapter facets;",
        coupon_mesh.CountFacets,
        "coupon facets",
    )

    for file_suffix, screw_x, screen_x in SELFIE_VARIANTS:
        export_selfie_variant(file_suffix, screw_x, screen_x)


# FreeCADCmd loads a passed .py file as a module rather than as ``__main__``.
# This file is an executable generator, so running/importing it is intentional.
main()
