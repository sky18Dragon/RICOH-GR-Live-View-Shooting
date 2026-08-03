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

# Two-piece hinged selfie adapter. A three-knuckle hinge uses ordinary 1.75 mm
# filament as its removable pin. Axial gaps exceed the 0.3 mm starting clearance
# Prusa recommends for moving FDM parts, while the 2.0 mm bore is a commonly
# proven light-friction fit for nominal 1.75 mm filament.
HINGE_PIN_DIAMETER = 1.75
HINGE_BORE_DIAMETER = 2.0
HINGE_BORE_TEARDROP_RISE = (
    (math.sqrt(2.0) - 1.0) * HINGE_BORE_DIAMETER / 2.0
)
HINGE_OUTER_DIAMETER = 5.5
HINGE_ORIGINAL_KNUCKLE_LENGTH = 8.0
HINGE_KNUCKLE_GAP = 0.3
HINGE_CENTER_KNUCKLE_LENGTH = SHOE_NECK_WIDTH
HINGE_ACTIVE_SPAN = (
    3 * HINGE_ORIGINAL_KNUCKLE_LENGTH + 2 * HINGE_KNUCKLE_GAP
)
HINGE_KNUCKLE_LENGTH = (
    HINGE_ACTIVE_SPAN
    - HINGE_CENTER_KNUCKLE_LENGTH
    - 2 * HINGE_KNUCKLE_GAP
) / 2.0
HINGE_CENTER_X0 = (
    STICK_LENGTH - HINGE_CENTER_KNUCKLE_LENGTH
) / 2.0
HINGE_LEFT_X0 = (
    HINGE_CENTER_X0 - HINGE_KNUCKLE_GAP - HINGE_KNUCKLE_LENGTH
)
HINGE_RIGHT_X0 = (
    HINGE_CENTER_X0
    + HINGE_CENTER_KNUCKLE_LENGTH
    + HINGE_KNUCKLE_GAP
)
HINGE_SHOE_Y_MIN = PLATE_Y_START + PLATE_WIDTH - SHOE_FOOT_LENGTH
HINGE_RADIUS = HINGE_OUTER_DIAMETER / 2.0
HINGE_SHOE_TOP_Z = (
    SHOE_FOOT_THICKNESS - 0.05 + SHOE_NECK_HEIGHT + 0.10
)
HINGE_PLATE_Z0 = HINGE_SHOE_TOP_Z
# The camera body ahead of the shoe rises only 2.3 mm from the insert bottom,
# so the cylinder can sit directly above that envelope instead of above the
# full neck height.
HINGE_CYLINDER_BOTTOM_Z = 2.3
HINGE_AXIS_Z = HINGE_CYLINDER_BOTTOM_Z + HINGE_RADIUS
HINGE_MOVING_PLATE_Z0 = HINGE_CYLINDER_BOTTOM_Z
# Place every hinge cylinder fully ahead of the hot-shoe retaining rails. The
# 0.2 mm gap is deliberate print/assembly tolerance beyond the nominal shoe
# front edge; the folded screen consequently sits slightly forward of center.
HINGE_SHOE_CENTER_Y = HINGE_SHOE_Y_MIN + SHOE_FOOT_LENGTH / 2.0
HINGE_FOLDED_PLATE_CENTER_OFFSET = (
    HINGE_MOVING_PLATE_Z0 + STICK_WIDTH / 2.0 - HINGE_AXIS_Z
)
HINGE_SHOE_FRONT_CLEARANCE = 0.2
HINGE_AXIS_Y = (
    HINGE_SHOE_Y_MIN - HINGE_RADIUS - HINGE_SHOE_FRONT_CLEARANCE
)
HINGE_MOVING_PLATE_Y_MIN = HINGE_AXIS_Y - HINGE_RADIUS
HINGE_FORWARD_EXTENSION = HINGE_SHOE_Y_MIN - HINGE_MOVING_PLATE_Y_MIN
HINGE_FOLDED_PLATE_CENTER_Y = (
    HINGE_AXIS_Y + HINGE_FOLDED_PLATE_CENTER_OFFSET
)
HINGE_FOLDED_CENTER_FORWARD_SHIFT = (
    HINGE_SHOE_CENTER_Y - HINGE_FOLDED_PLATE_CENTER_Y
)
# The two moving knuckles rotate with the screen plate.  In the -90-degree
# folded position their +Y cap becomes the camera-facing bottom surface.  Shave
# only that cap so it clears the camera/hot-shoe plate without raising or
# relocating the hinge axis.
HINGE_FOLDED_KNUCKLE_CLEARANCE_Z = 2.8
HINGE_FOLDED_KNUCKLE_MAX_Y = (
    HINGE_AXIS_Y + HINGE_AXIS_Z - HINGE_FOLDED_KNUCKLE_CLEARANCE_Z
)
HINGE_FOLDED_KNUCKLE_SHAVE = (
    HINGE_AXIS_Y + HINGE_RADIUS - HINGE_FOLDED_KNUCKLE_MAX_Y
)
HINGE_CENTER_CLEARANCE = 0.4
HINGE_OPEN_STOP_PAD_CLEARANCE = 0.06
HINGE_OPEN_STOP_PAD_DEPTH = 0.8
HINGE_MOUNT_PLATE_CORNER_RADIUS = (STICK_WIDTH - SCREW_SPACING) / 2.0
HINGE_MOUNT_PLATE_END_MARGIN = HINGE_MOUNT_PLATE_CORNER_RADIUS
HINGE_MOUNT_PLATE_X0 = SCREW_FROM_SHORT_EDGE - HINGE_MOUNT_PLATE_END_MARGIN
HINGE_MOUNT_PLATE_X1 = (
    STICK_LENGTH - SCREW_FROM_SHORT_EDGE + HINGE_MOUNT_PLATE_END_MARGIN
)
HINGE_MOUNT_PLATE_LENGTH = HINGE_MOUNT_PLATE_X1 - HINGE_MOUNT_PLATE_X0
HINGE_BASE_PLANE_WIDTH = HINGE_CENTER_KNUCKLE_LENGTH
HINGE_BASE_PLANE_X0 = HINGE_CENTER_X0
# A hair under the neck's 0.7 mm radius keeps a robust square stop land at the
# rear inner corners while remaining visually continuous with the neck.
HINGE_BASE_CORNER_RADIUS = 0.65
HINGE_PIN_CUT_LENGTH = HINGE_ACTIVE_SPAN + 1.6
HINGE_INTERNAL_ENTRY_DIAMETER = 2.5
HINGE_INTERNAL_ENTRY_DEPTH = 0.3
# Carry each camera-side attachment web almost to the hinge axis. This gives
# the fixed center knuckle a broad, load-bearing bond instead of relying on a
# thin overlap at the rear tangent of the barrel.
HINGE_BASE_WEB_AXIS_OFFSET = 0.25


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


def rounded_foot(cx, cy, z0, width, length, height, radius):
    """Create an XY rounded rectangle extruded upward from Z."""
    if radius <= 0.0 or 2.0 * radius > min(width, length):
        raise ValueError("Rounded-foot radius does not fit its rectangle")

    x0, x1 = cx - width / 2.0, cx + width / 2.0
    y0, y1 = cy - length / 2.0, cy + length / 2.0
    shape = Part.makeBox(
        width - 2.0 * radius,
        length,
        height,
        Vector(x0 + radius, y0, z0),
    ).fuse(
        Part.makeBox(
            width,
            length - 2.0 * radius,
            height,
            Vector(x0, y0 + radius, z0),
        )
    )
    for corner_x in (x0 + radius, x1 - radius):
        for corner_y in (y0 + radius, y1 - radius):
            shape = shape.fuse(
                Part.makeCylinder(
                    radius,
                    height,
                    Vector(corner_x, corner_y, z0),
                )
            )
    return shape.removeSplitter()


def rounded_hinge_chamfered_far_foot(
    cx, cy, z0, width, length, height, near_radius, far_chamfer
):
    """Extrude an XY profile with rounded hinge corners and 45-degree far corners."""
    if near_radius <= 0.0 or far_chamfer <= 0.0:
        raise ValueError("Hybrid foot corner sizes must be positive")
    if 2.0 * max(near_radius, far_chamfer) > min(width, length):
        raise ValueError("Hybrid foot corner sizes do not fit its rectangle")

    x0, x1 = cx - width / 2.0, cx + width / 2.0
    y0, y1 = cy - length / 2.0, cy + length / 2.0
    diagonal = near_radius / math.sqrt(2.0)
    near_left = Vector(x0 + near_radius, y0, z0)
    near_right = Vector(x1 - near_radius, y0, z0)
    right_near = Vector(x1, y0 + near_radius, z0)
    left_near = Vector(x0, y0 + near_radius, z0)
    edges = [
        Part.makeLine(near_left, near_right),
        Part.Arc(
            near_right,
            Vector(
                x1 - near_radius + diagonal,
                y0 + near_radius - diagonal,
                z0,
            ),
            right_near,
        ).toShape(),
        Part.makeLine(right_near, Vector(x1, y1 - far_chamfer, z0)),
        Part.makeLine(
            Vector(x1, y1 - far_chamfer, z0),
            Vector(x1 - far_chamfer, y1, z0),
        ),
        Part.makeLine(
            Vector(x1 - far_chamfer, y1, z0),
            Vector(x0 + far_chamfer, y1, z0),
        ),
        Part.makeLine(
            Vector(x0 + far_chamfer, y1, z0),
            Vector(x0, y1 - far_chamfer, z0),
        ),
        Part.makeLine(Vector(x0, y1 - far_chamfer, z0), left_near),
        Part.Arc(
            left_near,
            Vector(
                x0 + near_radius - diagonal,
                y0 + near_radius - diagonal,
                z0,
            ),
            near_left,
        ).toShape(),
    ]
    return Part.Face(Part.Wire(edges)).extrude(Vector(0, 0, height))


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


def rounded_wall(x0, y0, z0, length, depth, height, radius):
    """Extrude a true rounded rectangle in the XZ plane along Y."""
    if radius <= 0.0 or 2.0 * radius > min(length, height):
        raise ValueError("Rounded-wall radius does not fit its rectangle")

    x1, z1 = x0 + length, z0 + height
    diagonal = radius / math.sqrt(2.0)
    bottom_left = Vector(x0 + radius, y0, z0)
    bottom_right = Vector(x1 - radius, y0, z0)
    right_bottom = Vector(x1, y0, z0 + radius)
    right_top = Vector(x1, y0, z1 - radius)
    top_right = Vector(x1 - radius, y0, z1)
    top_left = Vector(x0 + radius, y0, z1)
    left_top = Vector(x0, y0, z1 - radius)
    left_bottom = Vector(x0, y0, z0 + radius)

    edges = [
        Part.makeLine(bottom_left, bottom_right),
        Part.Arc(
            bottom_right,
            Vector(x1 - radius + diagonal, y0, z0 + radius - diagonal),
            right_bottom,
        ).toShape(),
        Part.makeLine(right_bottom, right_top),
        Part.Arc(
            right_top,
            Vector(x1 - radius + diagonal, y0, z1 - radius + diagonal),
            top_right,
        ).toShape(),
        Part.makeLine(top_right, top_left),
        Part.Arc(
            top_left,
            Vector(x0 + radius - diagonal, y0, z1 - radius + diagonal),
            left_top,
        ).toShape(),
        Part.makeLine(left_top, left_bottom),
        Part.Arc(
            left_bottom,
            Vector(x0 + radius - diagonal, y0, z0 + radius - diagonal),
            bottom_left,
        ).toShape(),
    ]
    return Part.Face(Part.Wire(edges)).extrude(Vector(0, depth, 0))


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


def hinge_segment_start(index):
    starts = {
        1: HINGE_LEFT_X0,
        2: HINGE_CENTER_X0,
        3: HINGE_RIGHT_X0,
    }
    return starts[index]


def hinge_segment_length(index):
    return (
        HINGE_CENTER_KNUCKLE_LENGTH
        if index == 2
        else HINGE_KNUCKLE_LENGTH
    )


def make_hinge_barrels(indices):
    barrels = None
    for index in indices:
        barrel = Part.makeCylinder(
            HINGE_RADIUS,
            hinge_segment_length(index),
            Vector(hinge_segment_start(index), HINGE_AXIS_Y, HINGE_AXIS_Z),
            Vector(1, 0, 0),
        )
        barrels = barrel if barrels is None else barrels.fuse(barrel)
    return barrels


def hinge_feed_paths():
    """Continue the straight hinge bore into one side of each countersink."""
    left = [
        Vector(
            SELFIE_SCREW_X_NEAR,
            HINGE_AXIS_Y,
            HINGE_AXIS_Z,
        ),
        Vector(
            hinge_segment_start(1) + 0.3,
            HINGE_AXIS_Y,
            HINGE_AXIS_Z,
        ),
    ]
    right = [
        Vector(
            SELFIE_SCREW_X_MIRRORED,
            HINGE_AXIS_Y,
            HINGE_AXIS_Z,
        ),
        Vector(
            hinge_segment_start(3) + hinge_segment_length(3) - 0.3,
            HINGE_AXIS_Y,
            HINGE_AXIS_Z,
        ),
    ]
    return left, right


def make_rounded_path(points, radius):
    """Create a printable channel/pin from overlapping polyline segments.

    Extending each cylinder slightly past the path vertex gives the fused
    result real shared volume at bends.  This avoids the coincident spherical
    seams that some STL meshers otherwise leave as non-manifold edges.
    """
    result = None
    for start, end in zip(points, points[1:]):
        delta = end.sub(start)
        unit = Vector(delta)
        unit.normalize()
        overlap = min(0.12, delta.Length * 0.2)
        segment = Part.makeCylinder(
            radius,
            delta.Length + 2.0 * overlap,
            start.sub(unit.multiply(overlap)),
            delta,
        )
        result = segment if result is None else result.fuse(segment)
    return result.removeSplitter()


def hinge_bore_cutter(index, point_toward_negative_y):
    """Return a round bore with a shallow, outward-only 45-degree roof.

    The cutter is the union of the full nominal circle and a small tangent
    triangle. It can only add clearance, never intrude into the 2.0 mm pin
    passage. Each printed half points the cap away from its own build plate.
    """
    bore_radius = HINGE_BORE_DIAMETER / 2.0
    length = hinge_segment_length(index) + 0.06
    x0 = hinge_segment_start(index) - 0.03
    round_bore = Part.makeCylinder(
        bore_radius,
        length,
        Vector(x0, HINGE_AXIS_Y, HINGE_AXIS_Z),
        Vector(1, 0, 0),
    )

    apex_distance = bore_radius + HINGE_BORE_TEARDROP_RISE
    discriminant = (
        bore_radius * bore_radius
        - 2.0 * bore_radius * HINGE_BORE_TEARDROP_RISE
        - HINGE_BORE_TEARDROP_RISE * HINGE_BORE_TEARDROP_RISE
    )
    if discriminant < -1e-9:
        raise RuntimeError("Hinge teardrop rise is too large for a 45-degree cap")
    shoulder_half_width = (
        apex_distance - math.sqrt(max(0.0, discriminant))
    ) / 2.0
    shoulder_distance = math.sqrt(
        bore_radius * bore_radius - shoulder_half_width * shoulder_half_width
    )
    direction = -1.0 if point_toward_negative_y else 1.0
    lower_shoulder = Vector(
        x0,
        HINGE_AXIS_Y + direction * shoulder_distance,
        HINGE_AXIS_Z - shoulder_half_width,
    )
    apex = Vector(
        x0,
        HINGE_AXIS_Y + direction * apex_distance,
        HINGE_AXIS_Z,
    )
    upper_shoulder = Vector(
        x0,
        HINGE_AXIS_Y + direction * shoulder_distance,
        HINGE_AXIS_Z + shoulder_half_width,
    )
    profile = Part.makePolygon(
        [lower_shoulder, apex, upper_shoulder, lower_shoulder]
    )
    cap = Part.Face(profile).extrude(Vector(length, 0, 0))
    return round_bore.fuse(cap).removeSplitter()


def cut_hinge_bore(shape, indices, point_toward_negative_y):
    bore_radius = HINGE_BORE_DIAMETER / 2.0
    bore = None
    for index in indices:
        segment_bore = hinge_bore_cutter(index, point_toward_negative_y)
        bore = segment_bore if bore is None else bore.fuse(segment_bore)
    result = shape.cut(bore)

    # Small lead-ins on every knuckle prevent the flexible filament pin from
    # catching at an internal seam if the two printed parts are slightly
    # misaligned.
    internal_radius = HINGE_INTERNAL_ENTRY_DIAMETER / 2.0
    for index in indices:
        x0 = hinge_segment_start(index)
        x1 = x0 + hinge_segment_length(index)
        left_lead = Part.makeCone(
            internal_radius,
            bore_radius,
            HINGE_INTERNAL_ENTRY_DEPTH,
            Vector(x0 - 0.01, HINGE_AXIS_Y, HINGE_AXIS_Z),
            Vector(1, 0, 0),
        )
        right_lead = Part.makeCone(
            internal_radius,
            bore_radius,
            HINGE_INTERNAL_ENTRY_DEPTH,
            Vector(x1 + 0.01, HINGE_AXIS_Y, HINGE_AXIS_Z),
            Vector(-1, 0, 0),
        )
        result = result.cut(left_lead.fuse(right_lead))

    return result.removeSplitter()


def orient_y_min_face_for_print(shape):
    """Lay an assembled-coordinate hinge part on its common Y-min face."""
    print_shape = shape.copy()
    print_shape.rotate(Vector(0, 0, 0), Vector(1, 0, 0), 90.0)
    print_shape.translate(
        Vector(
            -print_shape.BoundBox.XMin,
            -print_shape.BoundBox.YMin,
            -print_shape.BoundBox.ZMin,
        )
    )
    return print_shape


def orient_y_max_face_for_print(shape):
    """Lay an assembled-coordinate hinge part on its common Y-max face."""
    print_shape = shape.copy()
    print_shape.rotate(Vector(0, 0, 0), Vector(1, 0, 0), -90.0)
    print_shape.translate(
        Vector(
            -print_shape.BoundBox.XMin,
            -print_shape.BoundBox.YMin,
            -print_shape.BoundBox.ZMin,
        )
    )
    return print_shape


def orient_z_min_face_for_print(shape):
    """Lay an assembled-coordinate part flat on its Z-min face."""
    print_shape = shape.copy()
    print_shape.translate(
        Vector(
            -print_shape.BoundBox.XMin,
            -print_shape.BoundBox.YMin,
            -print_shape.BoundBox.ZMin,
        )
    )
    return print_shape


def make_hinged_hotshoe_base():
    """Create the camera-side hinge half with the proven ISO 518 foot."""
    stick_cx = STICK_LENGTH / 2.0
    shoe_cy = PLATE_Y_START + PLATE_WIDTH - SHOE_FOOT_LENGTH / 2.0
    plate_z = SHOE_FOOT_THICKNESS + SHOE_NECK_HEIGHT

    foot = rounded_hinge_chamfered_far_foot(
        stick_cx,
        shoe_cy,
        0.0,
        SHOE_FOOT_WIDTH,
        SHOE_FOOT_LENGTH,
        SHOE_FOOT_THICKNESS,
        1.0,
        1.0,
    )
    neck = rounded_hinge_chamfered_far_foot(
        stick_cx,
        shoe_cy,
        SHOE_FOOT_THICKNESS - 0.05,
        SHOE_NECK_WIDTH,
        SHOE_FOOT_LENGTH,
        SHOE_NECK_HEIGHT + 0.10,
        0.7,
        0.7,
    )
    base_indices = (2,)
    web_y0 = HINGE_AXIS_Y + HINGE_BASE_WEB_AXIS_OFFSET
    web_y1 = PLATE_Y_START + PLATE_WIDTH
    web_low_top = HINGE_AXIS_Z + 0.2
    ramp_y1 = HINGE_AXIS_Y + HINGE_RADIUS + 0.4
    ramp_run = ramp_y1 - web_y0
    ramp_rise = HINGE_AXIS_Z + HINGE_RADIUS - web_low_top
    if ramp_rise > ramp_run:
        raise RuntimeError("Camera-side hinge support ramp exceeds 45 degrees")

    barrels = make_hinge_barrels(base_indices)
    back_plane_y0 = ramp_y1 - 0.1
    back_plane = Part.makeBox(
        HINGE_BASE_PLANE_WIDTH,
        web_y1 - back_plane_y0,
        web_low_top - plate_z,
        Vector(HINGE_BASE_PLANE_X0, back_plane_y0, plate_z),
    )
    upper_structure = back_plane.fuse(barrels)

    # Fill the undercut beneath the camera-facing half of the fixed barrel.
    # This removes the fragile/unsupported valley visible in side view and
    # gives the horizontal barrel a continuous foundation for flat printing.
    barrel_bed = Part.makeBox(
        HINGE_BASE_PLANE_WIDTH,
        web_y0 - HINGE_MOVING_PLATE_Y_MIN + 0.1,
        HINGE_AXIS_Z - plate_z,
        Vector(
            HINGE_BASE_PLANE_X0,
            HINGE_MOVING_PLATE_Y_MIN,
            plate_z,
        ),
    )
    upper_structure = upper_structure.fuse(barrel_bed)

    # Fill the lowered barrel's rear underside down to the 2.3 mm camera
    # clearance plane. Without this bridge, the circular barrel only grazes
    # the shoe neck and leaves a thin concave divot at the load-bearing joint.
    lower_rear_gusset = Part.makeBox(
        HINGE_BASE_PLANE_WIDTH,
        HINGE_SHOE_Y_MIN - HINGE_AXIS_Y + 0.1,
        plate_z - HINGE_CYLINDER_BOTTOM_Z + 0.05,
        Vector(
            HINGE_BASE_PLANE_X0,
            HINGE_AXIS_Y,
            HINGE_CYLINDER_BOTTOM_Z,
        ),
    )
    upper_structure = upper_structure.fuse(lower_rear_gusset)

    # Fill the upper camera-facing quadrant too, producing the requested
    # square top-left side profile. The screen plate's matching rectangular
    # center slot clears this profile throughout its intended quarter turn.
    barrel_top_fill = Part.makeBox(
        HINGE_BASE_PLANE_WIDTH,
        HINGE_AXIS_Y - HINGE_MOVING_PLATE_Y_MIN,
        HINGE_RADIUS,
        Vector(
            HINGE_BASE_PLANE_X0,
            HINGE_MOVING_PLATE_Y_MIN,
            HINGE_AXIS_Z,
        ),
    )
    upper_structure = upper_structure.fuse(barrel_top_fill)

    for index in base_indices:
        x0 = hinge_segment_start(index)
        # A short sub-45-degree ramp joins the compact rear plane to the single
        # fixed center knuckle. The neighboring spaces remain open for the two
        # moving knuckles; after the ramp ends, its barrel profile only
        # contracts in the print direction.
        web_profile = Part.makePolygon(
            [
                Vector(x0, web_y0, plate_z),
                Vector(x0, ramp_y1, plate_z),
                Vector(x0, ramp_y1, web_low_top),
                Vector(x0, web_y0, HINGE_AXIS_Z + HINGE_RADIUS),
                Vector(x0, web_y0, plate_z),
            ]
        )
        ramp = Part.Face(web_profile).extrude(
            Vector(hinge_segment_length(index), 0, 0)
        )
        upper_structure = upper_structure.fuse(ramp)

    # Trim the complete upper structure with one rounded XY footprint. This
    # carries the neck's small corner radius continuously to the top without
    # changing the square YZ faces that act as the 0/90-degree stops.
    upper_structure_z0 = min(
        plate_z, HINGE_AXIS_Z - HINGE_RADIUS
    ) - 0.1
    upper_rounding_mask = rounded_hinge_chamfered_far_foot(
        stick_cx,
        (HINGE_MOVING_PLATE_Y_MIN + web_y1) / 2.0,
        upper_structure_z0,
        HINGE_BASE_PLANE_WIDTH,
        web_y1 - HINGE_MOVING_PLATE_Y_MIN,
        HINGE_AXIS_Z + HINGE_RADIUS - upper_structure_z0 + 0.1,
        HINGE_BASE_CORNER_RADIUS,
        HINGE_BASE_CORNER_RADIUS,
    )
    upper_structure = upper_structure.common(upper_rounding_mask)
    base = foot.fuse(neck).fuse(upper_structure).removeSplitter()
    # Printed on Y-max: print-up is -Y in assembly coordinates.
    return cut_hinge_bore(base, base_indices, True)


def make_hinged_screen_plate():
    """Create one screen plate supporting both mirrored StickS3 orientations."""
    plate = rounded_wall(
        HINGE_MOUNT_PLATE_X0,
        HINGE_MOVING_PLATE_Y_MIN,
        HINGE_MOVING_PLATE_Z0,
        HINGE_MOUNT_PLATE_LENGTH,
        PLATE_THICKNESS,
        STICK_WIDTH,
        HINGE_MOUNT_PLATE_CORNER_RADIUS,
    )

    plate_indices = (1, 3)
    barrels = make_hinge_barrels(plate_indices)
    if not 0.0 < HINGE_FOLDED_KNUCKLE_SHAVE < HINGE_OUTER_DIAMETER:
        raise RuntimeError("Folded knuckle shave does not intersect the barrels")
    barrel_bounds = barrels.BoundBox
    folded_clearance_trim = Part.makeBox(
        barrel_bounds.XLength + 2.0,
        HINGE_FOLDED_KNUCKLE_MAX_Y - barrel_bounds.YMin + 1.0,
        barrel_bounds.ZLength + 2.0,
        Vector(
            barrel_bounds.XMin - 1.0,
            barrel_bounds.YMin - 1.0,
            barrel_bounds.ZMin - 1.0,
        ),
    )
    barrels = barrels.common(folded_clearance_trim)
    plate = plate.fuse(barrels)

    # Cut one rectangular, edge-open slot between the moving barrels. Its flat
    # top and vertical sides match the fixed knuckle's squared profile and
    # provide the folded endpoint stop without a fragile connecting web.
    center_index = 2
    center_relief_x0 = (
        hinge_segment_start(center_index) - HINGE_KNUCKLE_GAP
    )
    center_relief_length = (
        hinge_segment_length(center_index) + 2 * HINGE_KNUCKLE_GAP
    )
    center_edge_opening = Part.makeBox(
        center_relief_length,
        PLATE_THICKNESS + 0.2,
        (
            HINGE_AXIS_Z
            + HINGE_RADIUS
            + HINGE_CENTER_CLEARANCE
            - HINGE_MOVING_PLATE_Z0
            + 0.1
        ),
        Vector(
            center_relief_x0,
            HINGE_MOVING_PLATE_Y_MIN - 0.1,
            HINGE_MOVING_PLATE_Z0 - 0.1,
        ),
    )
    plate = plate.cut(center_edge_opening)

    # Restore a shallow section of the slot roof as a recessed upright stop.
    # It replaces the visible ridge on the camera-side part while preserving
    # the full 0.4 mm running clearance across the rest of the center relief.
    stop_pad_z0 = (
        HINGE_AXIS_Z + HINGE_RADIUS + HINGE_OPEN_STOP_PAD_CLEARANCE
    )
    stop_pad = Part.makeBox(
        HINGE_CENTER_KNUCKLE_LENGTH,
        HINGE_OPEN_STOP_PAD_DEPTH,
        (
            HINGE_CENTER_CLEARANCE
            - HINGE_OPEN_STOP_PAD_CLEARANCE
            + 0.1
        ),
        Vector(
            HINGE_CENTER_X0,
            HINGE_MOVING_PLATE_Y_MIN,
            stop_pad_z0,
        ),
    )
    plate = plate.fuse(stop_pad)

    # Continue the hinge bore straight into the inner half of each lower
    # countersink. The countersink exposes the end of the guide for loading,
    # while its outer half and the plate edge remain intact and load-bearing.
    feed_bore_radius = HINGE_BORE_DIAMETER / 2.0
    left_feed_path, right_feed_path = hinge_feed_paths()
    feed_channel = make_rounded_path(
        left_feed_path, feed_bore_radius
    ).fuse(make_rounded_path(right_feed_path, feed_bore_radius))
    plate = plate.cut(feed_channel)

    screw_zs = (
        HINGE_MOVING_PLATE_Z0 + (STICK_WIDTH - SCREW_SPACING) / 2.0,
        HINGE_MOVING_PLATE_Z0 + (STICK_WIDTH + SCREW_SPACING) / 2.0,
    )
    for screw_x in (SELFIE_SCREW_X_NEAR, SELFIE_SCREW_X_MIRRORED):
        for screw_z in screw_zs:
            through = Part.makeCylinder(
                SCREW_DIAMETER / 2.0,
                PLATE_THICKNESS + 0.6,
                Vector(screw_x, HINGE_MOVING_PLATE_Y_MIN - 0.3, screw_z),
                Vector(0, 1, 0),
            )
            countersink = Part.makeCone(
                2.0,
                SCREW_DIAMETER / 2.0,
                1.11,
                Vector(
                    screw_x,
                    HINGE_MOVING_PLATE_Y_MIN + PLATE_THICKNESS + 0.01,
                    screw_z,
                ),
                Vector(0, -1, 0),
            )
            plate = plate.cut(through.fuse(countersink))

    # Printed on Y-min: print-up is +Y in assembly coordinates.
    return cut_hinge_bore(plate.removeSplitter(), plate_indices, False)


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


def add_hinge_parameters(doc):
    obj = add_parameters(doc)
    values = {
        "HingePinDiameter": HINGE_PIN_DIAMETER,
        "HingeBoreDiameter": HINGE_BORE_DIAMETER,
        "HingeBoreTeardropRise": HINGE_BORE_TEARDROP_RISE,
        "HingeOuterDiameter": HINGE_OUTER_DIAMETER,
        "HingeKnuckleLength": HINGE_KNUCKLE_LENGTH,
        "HingeCenterKnuckleLength": HINGE_CENTER_KNUCKLE_LENGTH,
        "HingeKnuckleGap": HINGE_KNUCKLE_GAP,
        "HingeSpan": HINGE_ACTIVE_SPAN,
        "HingeAxisY": HINGE_AXIS_Y,
        "HingeAxisZ": HINGE_AXIS_Z,
        "HingeCylinderBottomZ": HINGE_CYLINDER_BOTTOM_Z,
        "HingeMovingPlateZ0": HINGE_MOVING_PLATE_Z0,
        "HingeShoeFrontClearance": HINGE_SHOE_FRONT_CLEARANCE,
        "HingeFoldedCenterForwardShift": HINGE_FOLDED_CENTER_FORWARD_SHIFT,
        "HingeForwardExtension": HINGE_FORWARD_EXTENSION,
        "HingeFoldedKnuckleClearanceZ": HINGE_FOLDED_KNUCKLE_CLEARANCE_Z,
        "HingeFoldedKnuckleShave": HINGE_FOLDED_KNUCKLE_SHAVE,
        "HingeMovingPlateYMin": HINGE_MOVING_PLATE_Y_MIN,
        "HingePinCutLength": HINGE_PIN_CUT_LENGTH,
        "HingeCenterClearance": HINGE_CENTER_CLEARANCE,
        "HingeOpenStopPadClearance": HINGE_OPEN_STOP_PAD_CLEARANCE,
        "HingeMountPlateLength": HINGE_MOUNT_PLATE_LENGTH,
        "HingeMountPlateCornerRadius": HINGE_MOUNT_PLATE_CORNER_RADIUS,
        "HingeBasePlaneWidth": HINGE_BASE_PLANE_WIDTH,
    }
    for name, value in values.items():
        obj.addProperty("App::PropertyLength", name, "Hinged Adapter")
        setattr(obj, name, value)
    obj.addProperty("App::PropertyString", "Motion", "Hinged Adapter")
    obj.Motion = "90 degrees: display-up folded position to front-facing upright"


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


def export_hinged_adapter():
    doc = App.newDocument("RicohGR_StickS3_Hinged_Selfie_Adapter")
    add_hinge_parameters(doc)

    base_shape = make_hinged_hotshoe_base()
    plate_shape = make_hinged_screen_plate()
    for label, shape in (("base", base_shape), ("screen plate", plate_shape)):
        if not shape.isValid() or len(shape.Solids) != 1:
            volumes = ", ".join(f"{solid.Volume:.3f}" for solid in shape.Solids)
            errors = shape.check(True)
            raise RuntimeError(
                f"Hinged adapter {label} is not one valid solid "
                f"(valid={shape.isValid()}, solids={len(shape.Solids)}, "
                f"volumes=[{volumes}], errors={errors})"
            )

    hinge_center = Vector(0.0, HINGE_AXIS_Y, HINGE_AXIS_Z)
    folded_plate_center_y = HINGE_AXIS_Y + (
        HINGE_MOVING_PLATE_Z0 + STICK_WIDTH / 2.0 - HINGE_AXIS_Z
    )
    cylinder_rear_y = HINGE_AXIS_Y + HINGE_RADIUS
    required_rear_y = HINGE_SHOE_Y_MIN - HINGE_SHOE_FRONT_CLEARANCE
    if cylinder_rear_y > required_rear_y + 0.001:
        raise RuntimeError("Hinge cylinders overlap the hot-shoe retaining area")
    if abs(folded_plate_center_y - HINGE_FOLDED_PLATE_CENTER_Y) > 0.001:
        raise RuntimeError("Folded StickS3 forward position is inconsistent")

    folded_clearance_probe = plate_shape.copy()
    folded_clearance_probe.rotate(hinge_center, Vector(1, 0, 0), -90.0)
    folded_knuckle_bottom_z = folded_clearance_probe.BoundBox.ZMin
    if folded_knuckle_bottom_z < HINGE_FOLDED_KNUCKLE_CLEARANCE_Z - 0.001:
        raise RuntimeError(
            "Folded moving knuckles intrude below the camera clearance plane: "
            f"{folded_knuckle_bottom_z:.3f} mm"
        )
    if abs(
        folded_knuckle_bottom_z - HINGE_FOLDED_KNUCKLE_CLEARANCE_Z
    ) > 0.01:
        raise RuntimeError(
            "Folded moving-knuckle trim missed its target clearance: "
            f"{folded_knuckle_bottom_z:.3f} mm"
        )
    device_shape = Part.makeBox(
        STICK_LENGTH,
        15.0,
        STICK_WIDTH,
        Vector(
            0.0,
            HINGE_MOVING_PLATE_Y_MIN - 15.0,
            HINGE_MOVING_PLATE_Z0,
        ),
    )
    # Camera hot-shoe reference retained in the editable document for visual
    # placement checks. The forward-shifted hinge uses its recessed slot pad
    # for the upright endpoint instead of bearing on the camera metal.
    camera_hotshoe_stop = Part.makeBox(
        SHOE_FOOT_WIDTH,
        SHOE_FOOT_LENGTH,
        0.2,
        Vector(
            (STICK_LENGTH - SHOE_FOOT_WIDTH) / 2.0,
            HINGE_SHOE_Y_MIN,
            HINGE_PLATE_Z0 - 0.25,
        ),
    )

    probe_radius = HINGE_BORE_DIAMETER / 2.0 - 0.02
    left_probe_path, right_probe_path = hinge_feed_paths()
    pin_channel_probe = make_rounded_path(
        left_probe_path, probe_radius
    ).fuse(make_rounded_path(right_probe_path, probe_radius))
    pin_channel_probe = pin_channel_probe.fuse(
        Part.makeCylinder(
            probe_radius,
            HINGE_ACTIVE_SPAN,
            Vector(
                hinge_segment_start(1),
                HINGE_AXIS_Y,
                HINGE_AXIS_Z,
            ),
            Vector(1, 0, 0),
        )
    )
    pin_channel_obstruction = (
        base_shape.common(pin_channel_probe).Volume
        + plate_shape.common(pin_channel_probe).Volume
    )
    if pin_channel_obstruction > 0.001:
        raise RuntimeError(
            "Mounting plate obstructs the three-knuckle filament-pin channel"
        )

    max_part_collision = 0.0
    max_part_collision_angle = 0
    max_part_collision_bbox = None
    for angle in range(0, -91, -5):
        moved_plate = plate_shape.copy()
        moved_plate.rotate(hinge_center, Vector(1, 0, 0), angle)
        part_collision = base_shape.common(moved_plate).Volume
        if part_collision > max_part_collision:
            max_part_collision = part_collision
            max_part_collision_angle = angle
            max_part_collision_bbox = base_shape.common(moved_plate).BoundBox
    if max_part_collision > 0.001:
        raise RuntimeError(
            "Hinge collision through the 90-degree motion: "
            f"parts={max_part_collision} mm3 at {max_part_collision_angle} deg, "
            f"part_bbox={max_part_collision_bbox}"
        )

    folded_stop_probe_angle = -92.0
    folded_stop_probe = plate_shape.copy()
    folded_stop_probe.rotate(
        hinge_center, Vector(1, 0, 0), folded_stop_probe_angle
    )
    if base_shape.common(folded_stop_probe).Volume <= 0.001:
        raise RuntimeError("Squared folded stop did not engage beyond 90 degrees")

    # The two printed halves clear at 90 degrees, then the close-fitting square
    # center-slot roof bears directly on the fixed knuckle beyond the endpoint.
    stop_probe_angle = 2.0
    stop_probe = plate_shape.copy()
    stop_probe.rotate(hinge_center, Vector(1, 0, 0), stop_probe_angle)
    stop_contact_volume = base_shape.common(stop_probe).Volume
    if stop_contact_volume <= 0.001:
        raise RuntimeError("Printed upright stop did not engage beyond 90 degrees")

    base_obj = doc.addObject("Part::Feature", "HotshoeHingeBase")
    base_obj.Label = "RICOH GR Hot Shoe Hinge Base"
    base_obj.Shape = base_shape
    if base_obj.ViewObject:
        base_obj.ViewObject.ShapeColor = (0.18, 0.42, 0.78)

    plate_obj = doc.addObject("Part::Feature", "StickS3HingedScreenPlate")
    plate_obj.Label = "StickS3 Dual-Orientation Hinged Screen Plate"
    plate_obj.Shape = plate_shape
    if plate_obj.ViewObject:
        plate_obj.ViewObject.ShapeColor = (0.30, 0.66, 0.88)

    pin = doc.addObject("Part::Feature", "FilamentPinReference")
    pin.Label = f"Cut {HINGE_PIN_CUT_LENGTH:.0f} mm of 1.75 mm filament"
    pin_radius = HINGE_PIN_DIAMETER / 2.0
    pin.Shape = Part.makeCylinder(
        pin_radius,
        HINGE_PIN_CUT_LENGTH,
        Vector(
            hinge_segment_start(1)
            - (HINGE_PIN_CUT_LENGTH - HINGE_ACTIVE_SPAN) / 2.0,
            HINGE_AXIS_Y,
            HINGE_AXIS_Z,
        ),
        Vector(1, 0, 0),
    )
    if pin.ViewObject:
        pin.ViewObject.ShapeColor = (0.95, 0.45, 0.10)
        pin.ViewObject.Visibility = False

    folded = doc.addObject("Part::Feature", "FoldedPlateReference")
    folded.Label = "Screen plate folded flat (reference only)"
    folded_shape = plate_shape.copy()
    folded_shape.rotate(hinge_center, Vector(1, 0, 0), -90.0)
    folded.Shape = folded_shape
    if folded.ViewObject:
        folded.ViewObject.ShapeColor = (0.30, 0.66, 0.88)
        folded.ViewObject.Transparency = 75
        folded.ViewObject.Visibility = False

    envelope = doc.addObject("Part::Feature", "StickS3Envelope")
    envelope.Label = "StickS3 upright reference envelope"
    envelope.Shape = device_shape
    if envelope.ViewObject:
        envelope.ViewObject.ShapeColor = (0.24, 0.26, 0.30)
        envelope.ViewObject.Transparency = 78
        envelope.ViewObject.Visibility = False

    shoe_stop = doc.addObject("Part::Feature", "CameraHotshoeStopReference")
    shoe_stop.Label = "Camera metal hot-shoe stop surface (reference only)"
    shoe_stop.Shape = camera_hotshoe_stop
    if shoe_stop.ViewObject:
        shoe_stop.ViewObject.ShapeColor = (0.55, 0.57, 0.60)
        shoe_stop.ViewObject.Transparency = 65
        shoe_stop.ViewObject.Visibility = False

    doc.recompute()
    doc.saveAs(str(OUTPUT_DIR / "ricoh_gr_sticks3_hinged_selfie_adapter.FCStd"))

    base_stem = "ricoh_gr_sticks3_hinged_selfie_hotshoe_base"
    plate_stem = "ricoh_gr_sticks3_hinged_selfie_screen_plate"
    Part.export([base_obj], str(OUTPUT_DIR / f"{base_stem}.step"))
    Part.export([plate_obj], str(OUTPUT_DIR / f"{plate_stem}.step"))

    base_print = doc.addObject("Part::Feature", "HotshoeBasePrintOrientation")
    base_print.Label = "Hot-shoe base: far-edge STL print orientation"
    base_print.Shape = orient_y_max_face_for_print(base_shape)
    plate_print = doc.addObject("Part::Feature", "ScreenPlatePrintOrientation")
    plate_print.Label = "Screen plate: STL print orientation"
    plate_print.Shape = orient_y_min_face_for_print(plate_shape)
    if base_print.ViewObject:
        base_print.ViewObject.Visibility = False
    if plate_print.ViewObject:
        plate_print.ViewObject.Visibility = False
    doc.recompute()
    doc.save()

    Mesh.export([base_print], str(OUTPUT_DIR / f"{base_stem}.stl"))
    Mesh.export([plate_print], str(OUTPUT_DIR / f"{plate_stem}.stl"))

    base_mesh = Mesh.Mesh(str(OUTPUT_DIR / f"{base_stem}.stl"))
    plate_mesh = Mesh.Mesh(str(OUTPUT_DIR / f"{plate_stem}.stl"))
    if not base_mesh.isSolid() or not plate_mesh.isSolid():
        raise RuntimeError(
            "A hinged adapter STL is not a closed solid "
            f"(base={base_mesh.isSolid()}, plate={plate_mesh.isSolid()})"
        )
    print(
        "Generated hinged adapter:",
        base_mesh.CountFacets,
        "base facets;",
        plate_mesh.CountFacets,
        "screen-plate facets;",
        "pin channel clear; 0-90 degree part motion collision-free;",
        f"endpoint stops engage by +{stop_probe_angle:.0f}/"
        f"{folded_stop_probe_angle:.0f} degrees;",
        "hinge clears shoe front by",
        round(HINGE_SHOE_FRONT_CLEARANCE, 2),
        "mm; folded plate center shifts",
        round(HINGE_FOLDED_CENTER_FORWARD_SHIFT, 2),
        "mm forward; hinge extends",
        round(HINGE_FORWARD_EXTENSION, 2),
        "mm forward; folded moving knuckles clear the shoe base by",
        round(folded_knuckle_bottom_z, 2),
        "mm",
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

    export_hinged_adapter()


# FreeCADCmd loads a passed .py file as a module rather than as ``__main__``.
# This file is an executable generator, so running/importing it is intentional.
main()
