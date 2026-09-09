bl_info = {
    "name": "Clay Mesh Generator",
    "author": "GJ26 Team",
    "version": (1, 0, 0),
    "blender": (3, 0, 0),
    "location": "View3D > Sidebar > Clay Mesh",
    "description": "Generate beveled voxel meshes from stage data",
    "category": "Import-Export",
}

import bpy
import json
import csv
import math
import os
from pathlib import Path
from bpy.props import StringProperty, FloatProperty, IntProperty
from bpy_extras.io_utils import ImportHelper, ExportHelper

# Constants
MAP_CHIP_EMPTY = 0
MAP_CHIP_CLAY = 1
MAP_CHIP_GOAL_PIECE = 2
MAP_CHIP_GOAL = 3
MAP_CHIP_GOAL_PIECE_UPPER = 4

FACE_NORMALS = [
    (1, 0, 0),   # 0: +X
    (-1, 0, 0),  # 1: -X
    (0, 1, 0),   # 2: +Y
    (0, -1, 0),  # 3: -Y
    (0, 0, 1),   # 4: +Z
    (0, 0, -1),  # 5: -Z
]

FACE_OFFSETS = [
    (1, 0, 0),
    (-1, 0, 0),
    (0, 1, 0),
    (0, -1, 0),
    (0, 0, 1),
    (0, 0, -1),
]

EDGE_DEFS = [
    (0, 2), (0, 3), (1, 2), (1, 3),
    (0, 4), (0, 5), (1, 4), (1, 5),
    (2, 4), (2, 5), (3, 4), (3, 5),
]

CORNER_DEFS = [
    (0, 2, 4), (0, 2, 5), (0, 3, 4), (0, 3, 5),
    (1, 2, 4), (1, 2, 5), (1, 3, 4), (1, 3, 5),
]


def face_axis(face):
    return face // 2


def face_sign(face):
    return 1 if face % 2 == 0 else -1


def edge_corner(faceA, faceB):
    nA = FACE_NORMALS[faceA]
    nB = FACE_NORMALS[faceB]
    return tuple((nA[i] + nB[i]) * 0.5 for i in range(3))


def corner_position(faceA, faceB, faceC):
    nA = FACE_NORMALS[faceA]
    nB = FACE_NORMALS[faceB]
    nC = FACE_NORMALS[faceC]
    return tuple((nA[i] + nB[i] + nC[i]) * 0.5 for i in range(3))


def edge_axis_index(faceA, faceB):
    return 3 - face_axis(faceA) - face_axis(faceB)


def find_corner_index(fA, fB, fC):
    faces = sorted([fA, fB, fC])
    for c, (ca, cb, cc) in enumerate(CORNER_DEFS):
        if sorted([ca, cb, cc]) == faces:
            return c
    return -1


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def dot(a, b):
    return sum(a[i] * b[i] for i in range(3))


def vec_add(a, b):
    return tuple(a[i] + b[i] for i in range(3))


def vec_sub(a, b):
    return tuple(a[i] - b[i] for i in range(3))


def vec_scale(a, s):
    return tuple(a[i] * s for i in range(3))


class BlockData:
    def __init__(self, chips, sizeX, sizeY, sizeZ):
        self.chips = chips
        self.sizeX = sizeX
        self.sizeY = sizeY
        self.sizeZ = sizeZ

    def flat_index(self, x, y, z):
        return x + self.sizeX * (z + self.sizeZ * y)

    def is_inside(self, x, y, z):
        return 0 <= x < self.sizeX and 0 <= y < self.sizeY and 0 <= z < self.sizeZ

    def is_clay(self, x, y, z):
        if not self.is_inside(x, y, z):
            return False
        flat = self.flat_index(x, y, z)
        return self.chips[flat] == MAP_CHIP_CLAY


def classify_cell(block, cx, cy, cz):
    face_exposed = [False] * 6
    edge_type = [None] * 12
    corner_type = [None] * 8

    for f in range(6):
        off = FACE_OFFSETS[f]
        face_exposed[f] = not block.is_clay(cx + off[0], cy + off[1], cz + off[2])

    for e, (fA, fB) in enumerate(EDGE_DEFS):
        offA = FACE_OFFSETS[fA]
        offB = FACE_OFFSETS[fB]
        nA = block.is_clay(cx + offA[0], cy + offA[1], cz + offA[2])
        nB = block.is_clay(cx + offB[0], cy + offB[1], cz + offB[2])
        nAB = block.is_clay(
            cx + offA[0] + offB[0],
            cy + offA[1] + offB[1],
            cz + offA[2] + offB[2],
        )

        if not nA and not nB:
            edge_type[e] = "Convex"
        elif (nA and not nB and nAB) or (not nA and nB and nAB):
            edge_type[e] = "Concave"
        else:
            edge_type[e] = None

    for c, (fA, fB, fC) in enumerate(CORNER_DEFS):
        offA = FACE_OFFSETS[fA]
        offB = FACE_OFFSETS[fB]
        offC = FACE_OFFSETS[fC]
        nA = block.is_clay(cx + offA[0], cy + offA[1], cz + offA[2])
        nB = block.is_clay(cx + offB[0], cy + offB[1], cz + offB[2])
        nC = block.is_clay(cx + offC[0], cy + offC[1], cz + offC[2])
        nAB = block.is_clay(
            cx + offA[0] + offB[0],
            cy + offA[1] + offB[1],
            cz + offA[2] + offB[2],
        )
        nAC = block.is_clay(
            cx + offA[0] + offC[0],
            cy + offA[1] + offC[1],
            cz + offA[2] + offC[2],
        )
        nBC = block.is_clay(
            cx + offB[0] + offC[0],
            cy + offB[1] + offC[1],
            cz + offB[2] + offC[2],
        )
        nABC = block.is_clay(
            cx + offA[0] + offB[0] + offC[0],
            cy + offA[1] + offB[1] + offC[1],
            cz + offA[2] + offB[2] + offC[2],
        )

        if not nA and not nB and not nC and not nAB and not nAC and not nBC:
            corner_type[c] = "Convex"
        elif nA and nB and nC and nAB and nAC and nBC and not nABC:
            corner_type[c] = "Concave"
        else:
            corner_type[c] = None

    return face_exposed, edge_type, corner_type


def get_face_insets(ctx, face, bevel_radius):
    axis = face_axis(face)
    u_axis = -1
    v_axis = -1
    for a in range(3):
        if a != axis:
            if u_axis < 0:
                u_axis = a
            else:
                v_axis = a

    u_min, u_max = -0.5, 0.5
    v_min, v_max = -0.5, 0.5

    face_exposed, edge_type, corner_type = ctx

    for e, (fA, fB) in enumerate(EDGE_DEFS):
        if edge_type[e] is None:
            continue
        if fA != face and fB != face:
            continue
        other_face = fB if fA == face else fA
        other_axis = face_axis(other_face)
        other_sign = face_sign(other_face)

        if other_axis == u_axis:
            if other_sign > 0:
                u_max -= bevel_radius
            else:
                u_min += bevel_radius
        elif other_axis == v_axis:
            if other_sign > 0:
                v_max -= bevel_radius
            else:
                v_min += bevel_radius

    return u_min, u_max, v_min, v_max


def generate_face(vertices, indices, ctx, face, cell_offset, bevel_radius):
    normal = FACE_NORMALS[face]
    axis = face_axis(face)

    u_axis = -1
    v_axis = -1
    for a in range(3):
        if a != axis:
            if u_axis < 0:
                u_axis = a
            else:
                v_axis = a

    u_min, u_max, v_min, v_max = get_face_insets(ctx, face, bevel_radius)

    base = vec_add(cell_offset, vec_scale(normal, 0.5))

    u_dir = [0, 0, 0]
    v_dir = [0, 0, 0]
    u_dir[u_axis] = 1.0
    v_dir[v_axis] = 1.0

    base_idx = len(vertices)

    vertices.append(
        (
            vec_add(vec_add(base, vec_scale(u_dir, u_min)), vec_scale(v_dir, v_min)),
            (0, 0),
            normal,
        )
    )
    vertices.append(
        (
            vec_add(vec_add(base, vec_scale(u_dir, u_max)), vec_scale(v_dir, v_min)),
            (1, 0),
            normal,
        )
    )
    vertices.append(
        (
            vec_add(vec_add(base, vec_scale(u_dir, u_max)), vec_scale(v_dir, v_max)),
            (1, 1),
            normal,
        )
    )
    vertices.append(
        (
            vec_add(vec_add(base, vec_scale(u_dir, u_min)), vec_scale(v_dir, v_max)),
            (0, 1),
            normal,
        )
    )

    winding_sign = dot(cross(u_dir, v_dir), normal)
    if winding_sign >= 0:
        indices.extend([base_idx, base_idx + 1, base_idx + 2])
        indices.extend([base_idx, base_idx + 2, base_idx + 3])
    else:
        indices.extend([base_idx, base_idx + 2, base_idx + 1])
        indices.extend([base_idx, base_idx + 3, base_idx + 2])


def generate_edge_fillet(
    vertices,
    indices,
    cell_offset,
    faceA,
    faceB,
    edge_type,
    bevel_radius,
    segment,
    start_offset,
    end_offset,
):
    nA = FACE_NORMALS[faceA]
    nB = FACE_NORMALS[faceB]
    edge = edge_corner(faceA, faceB)
    axis_idx = edge_axis_index(faceA, faceB)

    axis_dir = [0, 0, 0]
    axis_dir[axis_idx] = 1.0

    center = vec_sub(edge, vec_scale(vec_add(nA, nB), bevel_radius))

    angle_start = 0.0
    angle_end = math.pi / 2.0

    base_idx = len(vertices)

    for s in range(segment + 1):
        t = s / segment
        angle = angle_start + t * (angle_end - angle_start)
        cos_a = math.cos(angle)
        sin_a = math.sin(angle)
        dir_vec = vec_add(vec_scale(nA, cos_a), vec_scale(nB, sin_a))

        pos = vec_add(center, vec_scale(dir_vec, bevel_radius))
        normal = dir_vec

        vertices.append(
            (
                vec_add(
                    vec_add(cell_offset, pos), vec_scale(axis_dir, start_offset)
                ),
                (t, 0),
                normal,
            )
        )
        vertices.append(
            (
                vec_add(
                    vec_add(cell_offset, pos), vec_scale(axis_dir, end_offset)
                ),
                (t, 1),
                normal,
            )
        )

    edge_sign = dot(cross(nB, axis_dir), nA)
    flip = edge_sign < 0.0

    for s in range(segment):
        i0 = base_idx + s * 2
        i1 = i0 + 1
        i2 = i0 + 2
        i3 = i0 + 3

        if not flip:
            indices.extend([i0, i2, i1])
            indices.extend([i1, i2, i3])
        else:
            indices.extend([i0, i1, i2])
            indices.extend([i1, i3, i2])


def generate_corner_fillet(
    vertices, indices, cell_offset, faceA, faceB, faceC, corner_type, bevel_radius, segment
):
    nA = FACE_NORMALS[faceA]
    nB = FACE_NORMALS[faceB]
    nC = FACE_NORMALS[faceC]
    corner = corner_position(faceA, faceB, faceC)

    center = vec_sub(corner, vec_scale(vec_add(vec_add(nA, nB), nC), bevel_radius))

    base_idx = len(vertices)

    for j in range(segment + 1):
        phi = (j / segment) * (math.pi / 2.0)
        cp = math.cos(phi)
        sp = math.sin(phi)
        is_singular = j == segment
        i_max = 0 if is_singular else segment

        for i in range(i_max + 1):
            theta = (i / segment) * (math.pi / 2.0)
            ct = math.cos(theta)
            st = math.sin(theta)

            if is_singular:
                dir_vec = nC
            else:
                dir_vec = vec_add(
                    vec_add(vec_scale(nA, cp * ct), vec_scale(nB, cp * st)),
                    vec_scale(nC, sp),
                )

            pos = vec_add(center, vec_scale(dir_vec, bevel_radius))
            normal = dir_vec

            u = 0.5 if is_singular else i / segment
            v = j / segment

            vertices.append((vec_add(cell_offset, pos), (u, v), normal))

    corner_sign = dot(cross(nB, nC), nA)
    flip = corner_sign > 0.0

    row_start_indices = [0] * (segment + 1)
    current_idx = base_idx
    for j in range(segment + 1):
        row_start_indices[j] = current_idx
        if j < segment:
            current_idx += segment + 1
        else:
            current_idx += 1

    for j in range(segment):
        is_last_row = j == segment - 1

        for i in range(segment):
            i0 = row_start_indices[j] + i
            i1 = i0 + 1

            if is_last_row:
                i_singular = row_start_indices[j + 1]
                if not flip:
                    indices.extend([i0, i_singular, i1])
                else:
                    indices.extend([i0, i1, i_singular])
            else:
                i2 = row_start_indices[j + 1] + i
                i3 = i2 + 1

                if not flip:
                    indices.extend([i0, i2, i1])
                    indices.extend([i1, i2, i3])
                else:
                    indices.extend([i0, i1, i2])
                    indices.extend([i1, i3, i2])


def load_stage_data(stage_dir):
    stage_dir = Path(stage_dir)

    chips = []
    size_x = 0
    size_y = 0
    size_z = 0

    layer_files = sorted(stage_dir.glob("layer*.csv"))
    size_y = len(layer_files)

    for layer_idx, layer_file in enumerate(layer_files):
        with open(layer_file, "r") as f:
            reader = csv.reader(f)
            rows = list(reader)

        if layer_idx == 0:
            size_z = len(rows)
            size_x = len(rows[0]) if rows else 0

        for row in rows:
            for val in row:
                chips.append(int(val))

    stage_json_path = stage_dir / "stage.json"
    clay_records = []
    if stage_json_path.exists():
        with open(stage_json_path, "r") as f:
            stage_data = json.load(f)
        clay_records = stage_data.get("Clay", [])

    return chips, size_x, size_y, size_z, clay_records


def generate_block_mesh(chips, sizeX, sizeY, sizeZ, origin_flat, bevel_radius, segment):
    block = BlockData(chips, sizeX, sizeY, sizeZ)

    origin_x = origin_flat % sizeX
    origin_z = (origin_flat // sizeX) % sizeZ
    origin_y = origin_flat // (sizeX * sizeZ)

    vertices = []
    indices = []

    for y in range(sizeY):
        for z in range(sizeZ):
            for x in range(sizeX):
                flat = x + sizeX * (z + sizeZ * y)
                if chips[flat] != MAP_CHIP_CLAY:
                    continue
                # Check if this cell belongs to the target block
                # For simplicity, each clay cell is its own block
                if flat != origin_flat:
                    continue

                cell_offset = (x - origin_x, y - origin_y, z - origin_z)
                ctx = classify_cell(block, x, y, z)

                for f in range(6):
                    if ctx[0][f]:
                        generate_face(vertices, indices, ctx, f, cell_offset, bevel_radius)

                for e, (fA, fB) in enumerate(EDGE_DEFS):
                    if ctx[1][e] is None:
                        continue
                    axis_idx = edge_axis_index(fA, fB)
                    neg_face = 2 * axis_idx + 1
                    pos_face = 2 * axis_idx
                    neg_corner = find_corner_index(neg_face, fA, fB)
                    pos_corner = find_corner_index(pos_face, fA, fB)

                    start_offset = -0.5
                    end_offset = 0.5
                    if neg_corner >= 0 and ctx[2][neg_corner] is not None:
                        start_offset += bevel_radius
                    if pos_corner >= 0 and ctx[2][pos_corner] is not None:
                        end_offset -= bevel_radius

                    generate_edge_fillet(
                        vertices,
                        indices,
                        cell_offset,
                        fA,
                        fB,
                        ctx[1][e],
                        bevel_radius,
                        segment,
                        start_offset,
                        end_offset,
                    )

                for c, (fA, fB, fC) in enumerate(CORNER_DEFS):
                    if ctx[2][c] is None:
                        continue
                    generate_corner_fillet(
                        vertices,
                        indices,
                        cell_offset,
                        fA,
                        fB,
                        fC,
                        ctx[2][c],
                        bevel_radius,
                        segment,
                    )

    return vertices, indices


def create_blender_mesh(name, vertices, indices):
    mesh = bpy.data.meshes.new(name)

    verts = [(v[0][0], v[0][1], v[0][2]) for v in vertices]
    edges = []
    faces = []
    for i in range(0, len(indices), 3):
        faces.append((indices[i], indices[i + 1], indices[i + 2]))

    mesh.from_pydata(verts, edges, faces)
    mesh.update()

    # Add UV map
    uv_layer = mesh.uv_layers.new()
    for loop in mesh.loops:
        uv_layer.data[loop.index].uv = vertices[loop.vertex_index][1]

    # Add normals
    mesh.normals_split_custom_set_from_vertices(
        [v[2] for v in vertices]
    )
    mesh.use_auto_smooth = True

    return mesh


class CLAYMESH_OT_import_stage(bpy.types.Operator, ImportHelper):
    """Import stage data and generate clay mesh"""
    bl_idname = "claymesh.import_stage"
    bl_label = "Import Stage Data"
    bl_options = {"REGISTER", "UNDO"}

    filename_ext = ".csv"
    filter_glob: StringProperty(default="layer*.csv", options={"HIDDEN"})

    bevel_radius: FloatProperty(
        name="Bevel Radius",
        default=0.15,
        min=0.01,
        max=0.49,
    )
    segment: IntProperty(
        name="Segments",
        default=3,
        min=1,
        max=16,
    )

    def execute(self, context):
        filepath = Path(self.filepath)
        stage_dir = filepath.parent

        try:
            chips, size_x, size_y, size_z, clay_records = load_stage_data(stage_dir)
        except Exception as e:
            self.report({"ERROR"}, f"Failed to load stage data: {e}")
            return {"CANCELLED"}

        # Generate mesh for each clay block
        for flat in range(len(chips)):
            if chips[flat] != MAP_CHIP_CLAY:
                continue

            try:
                verts, inds = generate_block_mesh(
                    chips, size_x, size_y, size_z, flat, self.bevel_radius, self.segment
                )
                if verts:
                    mesh = create_blender_mesh(f"clay_block_{flat}", verts, inds)
                    obj = bpy.data.objects.new(f"clay_block_{flat}", mesh)
                    context.collection.objects.link(obj)
            except Exception as e:
                self.report({"WARNING"}, f"Failed to generate block {flat}: {e}")

        self.report({"INFO"}, f"Generated meshes from {stage_dir.name}")
        return {"FINISHED"}


class CLAYMESH_OT_export_obj(bpy.types.Operator, ExportHelper):
    """Export selected clay mesh as OBJ"""
    bl_idname = "claymesh.export_obj"
    bl_label = "Export OBJ"
    bl_options = {"REGISTER", "UNDO"}

    filename_ext = ".obj"
    filter_glob: StringProperty(default="*.obj", options={"HIDDEN"})

    def execute(self, context):
        obj = context.active_object
        if not obj or obj.type != "MESH":
            self.report({"ERROR"}, "No mesh object selected")
            return {"CANCELLED"}

        filepath = self.filepath
        mesh = obj.data

        with open(filepath, "w") as f:
            f.write("# Clay mesh exported by blender_clay_mesh.py\n")
            f.write("mtllib clay.mtl\n\n")

            for v in mesh.vertices:
                f.write(f"v {v.co.x:.6f} {v.co.y:.6f} {v.co.z:.6f}\n")

            if mesh.uv_layers:
                uv_layer = mesh.uv_layers.active
                for loop in mesh.loops:
                    uv = uv_layer.data[loop.index].uv
                    f.write(f"vt {uv.x:.6f} {uv.y:.6f}\n")

            if mesh.has_custom_normals:
                for v in mesh.vertices:
                    f.write(f"vn {v.normal.x:.6f} {v.normal.y:.6f} {v.normal.z:.6f}\n")

            f.write("\nusemtl clayMaterial\n")
            f.write("s off\n\n")

            for poly in mesh.polygons:
                for loop_idx in poly.loop_indices:
                    loop = mesh.loops[loop_idx]
                    vi = loop.vertex_index + 1
                    f.write(f"f {vi}/{vi}/{vi} ")
                f.write("\n")

        self.report({"INFO"}, f"Exported to {filepath}")
        return {"FINISHED"}


class CLAYMESH_PT_panel(bpy.types.Panel):
    """Clay Mesh Generator Panel"""
    bl_label = "Clay Mesh Generator"
    bl_idname = "CLAYMESH_PT_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Clay Mesh"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.operator("claymesh.import_stage", text="Import Stage & Generate")
        col.operator("claymesh.export_obj", text="Export Selected OBJ")

        box = layout.box()
        box.label(text="Settings:")
        box.prop(context.scene, "clay_bevel_radius")
        box.prop(context.scene, "clay_segment")


def register():
    bpy.utils.register_class(CLAYMESH_OT_import_stage)
    bpy.utils.register_class(CLAYMESH_OT_export_obj)
    bpy.utils.register_class(CLAYMESH_PT_panel)

    bpy.types.Scene.clay_bevel_radius = FloatProperty(
        name="Bevel Radius",
        default=0.15,
        min=0.01,
        max=0.49,
    )
    bpy.types.Scene.clay_segment = IntProperty(
        name="Segments",
        default=3,
        min=1,
        max=16,
    )


def unregister():
    del bpy.types.Scene.clay_bevel_radius
    del bpy.types.Scene.clay_segment
    bpy.utils.unregister_class(CLAYMESH_PT_panel)
    bpy.utils.unregister_class(CLAYMESH_OT_export_obj)
    bpy.utils.unregister_class(CLAYMESH_OT_import_stage)


if __name__ == "__main__":
    register()
