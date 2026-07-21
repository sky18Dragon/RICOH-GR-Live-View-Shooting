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


# FreeCADCmd loads a passed .py file as a module rather than as ``__main__``.
# This file is an executable generator, so running/importing it is intentional.
main()
