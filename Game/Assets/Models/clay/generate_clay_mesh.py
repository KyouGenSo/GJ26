"""
Clay Mesh Generator - Standalone Script
エディター保存時に自動呼び出され、Blenderをバックグラウンドで実行してメッシュを生成します。

Usage:
    blender --background --python generate_clay_mesh.py -- --stage-dir <path> --output-dir <path>
"""

import bpy
import bmesh
import json
import csv
import os
import sys
import argparse
from pathlib import Path
from collections import deque


# ClayColor::Textures と同じ並び（C++ 側 MapChipField.h の ClayColor 名前空間）
# Index 0 → "clay.png"、Index 1..4 → "clay2.png".."clay5.png"
CLAY_TEXTURES = [
    "clay.png",
    "clay2.png",
    "clay3.png",
    "clay4.png",
    "clay5.png",
]
CLAY_COLOR_COUNT = len(CLAY_TEXTURES)


def color_to_texture(color):
    """Color 番号 → テクスチャファイル名"""
    if 0 <= color < CLAY_COLOR_COUNT:
        return CLAY_TEXTURES[color]
    return CLAY_TEXTURES[0]


def parse_args():
    """コマンドライン引数を解析"""
    # Blenderは--の後にカスタム引数を渡す
    argv = sys.argv
    if "--" in argv:
        argv = argv[argv.index("--") + 1:]
    else:
        argv = []

    parser = argparse.ArgumentParser(description="Generate clay mesh from MapChip data")
    parser.add_argument("--stage-dir", required=True, help="Stage directory containing CSV layers and stage.json")
    parser.add_argument("--stage-id", required=True, help="Stage ID")
    parser.add_argument("--output-dir", required=True, help="Output directory for OBJ files")
    parser.add_argument("--bevel-radius", type=float, default=0.15, help="Bevel radius")
    parser.add_argument("--segments", type=int, default=3, help="Bevel segments")
    parser.add_argument("--texture-base-dir", default="Game/Assets/Models/clay",
                        help="Base directory where clay{N}.png textures live (relative to project root)")
    return parser.parse_args(argv)


def read_csv_layers(stage_dir):
    """CSVレイヤーファイルを読み込む"""
    layers = []
    y = 1
    while True:
        csv_path = Path(stage_dir) / f"layer{y:02d}.csv"
        if not csv_path.exists():
            break
        with open(csv_path, "r", newline="") as f:
            reader = csv.reader(f)
            layer = []
            for row in reader:
                layer.append([int(v) for v in row])
        layers.append(layer)
        y += 1
    return layers


def read_stage_json(stage_dir):
    """stage.jsonを読み込む"""
    json_path = Path(stage_dir) / "stage.json"
    if not json_path.exists():
        return {}
    with open(json_path, "r") as f:
        return json.load(f)


def read_clay_color_map(stage_dir, sizeX, sizeY, sizeZ):
    """
    stage.json から粘土セルごとの Color 番号を読み取り、flat index の配列で返す。
    粘土でないセル / JSON に無いセルは 0（既定の clay.png 扱い）。
    """
    color_map = [0] * (sizeX * sizeY * sizeZ)
    data = read_stage_json(stage_dir)
    clay_entries = data.get("Clay", [])
    for entry in clay_entries:
        pos = entry.get("Position", None)
        color = entry.get("Color", 0)
        if not pos or len(pos) < 3:
            continue
        x, y, z = int(pos[0]), int(pos[1]), int(pos[2])
        if not (0 <= x < sizeX and 0 <= y < sizeY and 0 <= z < sizeZ):
            continue
        flat = x + sizeX * (z + sizeZ * y)
        if 0 <= color < CLAY_COLOR_COUNT:
            color_map[flat] = color
    return color_map


def compute_connected_components(chips, sizeX, sizeY, sizeZ):
    """
    連結成分を計算してclayOriginを決定する
    各Clayセルに、属するブロックの原点（flat index）を割り当てる
    """
    clayOrigin = [-1] * len(chips)
    visited = [False] * len(chips)

    def flat_index(x, y, z):
        return x + sizeX * (z + sizeZ * y)

    def is_clay(idx):
        return 0 <= idx < len(chips) and chips[idx] == 1

    # 6方向の隣接
    directions = [
        (1, 0, 0), (-1, 0, 0),
        (0, 1, 0), (0, -1, 0),
        (0, 0, 1), (0, 0, -1),
    ]

    for start_idx in range(len(chips)):
        if chips[start_idx] != 1 or visited[start_idx]:
            continue

        # BFSで連結成分を見つける
        queue = deque([start_idx])
        visited[start_idx] = True
        component = [start_idx]

        while queue:
            idx = queue.popleft()
            x = idx % sizeX
            z = (idx // sizeX) % sizeZ
            y = idx // (sizeX * sizeZ)

            for dx, dy, dz in directions:
                nx, ny, nz = x + dx, y + dy, z + dz
                if 0 <= nx < sizeX and 0 <= ny < sizeY and 0 <= nz < sizeZ:
                    nidx = flat_index(nx, ny, nz)
                    if is_clay(nidx) and not visited[nidx]:
                        visited[nidx] = True
                        queue.append(nidx)
                        component.append(nidx)

        # 連結成分の最初のセルを原点とする
        origin = min(component)
        for idx in component:
            clayOrigin[idx] = origin

    return clayOrigin


def create_voxel_mesh(chips, clayOrigin, clayColorMap, sizeX, sizeY, sizeZ):
    """
    ボクセルメッシュを作成する
    各ブロック（連結成分）ごとにメッシュオブジェクトを生成
    隣接する立方体の面は距離で頂点をマージし、内部面を削除して1つの連結メッシュにする
    """
    blocks = {}      # origin -> list of (x, y, z)
    block_colors = {}  # origin -> color (ブロックの代表色)

    for idx, chip in enumerate(chips):
        if chip != 1:
            continue
        origin = clayOrigin[idx]
        if origin not in blocks:
            blocks[origin] = []
            block_colors[origin] = clayColorMap[idx]

        x = idx % sizeX
        z = (idx // sizeX) % sizeZ
        y = idx // (sizeX * sizeZ)
        blocks[origin].append((x, y, z))

    mesh_objects = []

    for origin, cells in blocks.items():
        # 起点セルの座標（エンジンの配置基準）
        # origin セルが mesh のローカル原点 (0, 0, 0) に来るようにする
        ox = origin % sizeX
        oz = (origin // sizeX) % sizeZ
        oy = origin // (sizeX * sizeZ)

        # ブロックメッシュを作成
        mesh = bpy.data.meshes.new(f"clay_block_{origin}")
        obj = bpy.data.objects.new(f"clay_block_{origin}", mesh)
        bpy.context.collection.objects.link(obj)

        # BMeshでボクセルを構築
        bm = bmesh.new()

        for (x, y, z) in cells:
            # 各セルを1x1x1のキューブとして追加
            # 起点セルからの相対位置（整数座標）→ キューブ中心が整数位置になる
            #
            # Blender OBJ エクスポーターのデフォルト変換 (X,Y,Z) → (X,Z,-Y) が
            # 適用されるため、それを考慮してマッピングする:
            #   px (Blender X) = -(stage x) → OBJ X (右、符号反転)     ✓
            #   py (Blender Y) = -(stage z) → OBJ Z (前、符号反転)     ✓
            #   pz (Blender Z) = stage y    → OBJ Y (上)              ✓
            px = -(x - ox)
            py = -(z - oz)
            pz = y - oy

            # キューブの8頂点
            verts = [
                (px - 0.5, py - 0.5, pz - 0.5),
                (px + 0.5, py - 0.5, pz - 0.5),
                (px + 0.5, py + 0.5, pz - 0.5),
                (px - 0.5, py + 0.5, pz - 0.5),
                (px - 0.5, py - 0.5, pz + 0.5),
                (px + 0.5, py - 0.5, pz + 0.5),
                (px + 0.5, py + 0.5, pz + 0.5),
                (px - 0.5, py + 0.5, pz + 0.5),
            ]

            bverts = [bm.verts.new(v) for v in verts]

            # 6面（外向き法線になるよう CCW ワインディング）
            faces = [
                [0, 3, 2, 1],  # -Z 面（外向き法線 -Z）
                [4, 5, 6, 7],  # +Z 面（外向き法線 +Z）
                [0, 1, 5, 4],  # -Y 面（底面、外向き法線 -Y）
                [2, 3, 7, 6],  # +Y 面（天井、外向き法線 +Y）
                [1, 2, 6, 5],  # +X 面（外向き法線 +X）
                [0, 4, 7, 3],  # -X 面（外向き法線 -X）
            ]

            for face in faces:
                bm.faces.new([bverts[i] for i in face])

        bm.normal_update()
        bm.verts.ensure_lookup_table()
        bm.faces.ensure_lookup_table()

        # 距離で重複頂点をマージ（隣接する立方体の頂点を共有化）
        bmesh.ops.remove_doubles(
            bm,
            verts=list(bm.verts),
            dist=0.001,
        )

        bm.normal_update()
        bm.faces.ensure_lookup_table()

        # 内部面を削除
        # 隣接する2つの立方体は同じ位置に2つの面を持つので、両方を削除する
        position_to_faces = {}
        for face in bm.faces:
            centroid = face.calc_center_median()
            # 丸めてキーを作成（浮動小数点誤差対策）
            key = (
                round(centroid.x, 3),
                round(centroid.y, 3),
                round(centroid.z, 3),
            )
            position_to_faces.setdefault(key, []).append(face)

        for key, faces in position_to_faces.items():
            if len(faces) == 2:
                # 同じ位置に2つの面 = 隣接立方体の共有面 = 内部面：両方を削除
                for face in faces:
                    bm.faces.remove(face)

        # 孤立した頂点と辺を削除（面に接続していないジオメトリを除去）
        bm.edges.ensure_lookup_table()
        bm.verts.ensure_lookup_table()

        # どの面にも属さない頂点を削除
        for vert in list(bm.verts):
            if not vert.link_faces:
                bm.verts.remove(vert)

        # 孤立辺を削除
        for edge in list(bm.edges):
            if not edge.link_faces:
                bm.edges.remove(edge)

        bm.normal_update()

        # 座標系は Blender OBJ エクスポーターのデフォルト変換に任せる
        # (頂点配置マッピングで対応済み、px=stage_x, py=-(stage_z), pz=stage_y)

        bm.normal_update()
        bm.to_mesh(mesh)
        bm.free()

        mesh.update()
        mesh_objects.append((origin, obj, block_colors[origin]))

    return mesh_objects


def apply_bevel(obj, bevel_radius, segments):
    """ベベルモディファイアを適用"""
    # ベベルモディファイアを追加
    mod = obj.modifiers.new(name="Bevel", type="BEVEL")
    mod.width = bevel_radius
    mod.segments = segments
    mod.profile = 0.5  # 円形
    mod.affect = "EDGES"
    mod.limit_method = "NONE"  # すべてのエッジをベベル

    # モディファイアを適用
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)


def triangulate_mesh(obj):
    """メッシュを全て三角面化する。
    ベベル後のメッシュには四角・Nゴン面が混在するため、
    描画の一貫性とシェーダ計算の安定化のため三角面に変換する。
    """
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.quads_convert_to_tris(quad_method='BEAUTY', ngon_method='BEAUTY')
    bpy.ops.object.mode_set(mode='OBJECT')
    obj.select_set(False)


def finalize_mesh(obj, texture_file_name):
    """
    メッシュを最終化する
    - スムーズシェードを適用
    - UV座標を生成（法線方向ベースのキューブプロジェクション）
    - 法線を再計算
    - テクスチャ付きのマテリアルを割り当て
    """
    mesh = obj.data

    # スムーズシェード: 全ポリゴンをスムーズシェーディングに
    for poly in mesh.polygons:
        poly.use_smooth = True

    # カスタム分割法線を更新（スムーズシェーディング用）
    mesh.update()

    # 既存のUV層を削除して新規追加
    while mesh.uv_layers:
        mesh.uv_layers.remove(mesh.uv_layers[0])
    uv_layer = mesh.uv_layers.new(name="UVMap")

    # 各ポリゴンの法線方向に応じてUVを投影（キューブプロジェクション風）
    axes = ('x', 'y', 'z')
    for poly in mesh.polygons:
        normal = poly.normal

        # 法線の支配軸を決定（最も大きい成分を持つ軸）
        abs_n = (abs(normal.x), abs(normal.y), abs(normal.z))
        max_idx = max(range(3), key=lambda i: abs_n[i])

        # UV軸を選択（法線と直交する2軸）
        if max_idx == 0:    # ±X面 → Y, Z軸を使用
            u_idx, v_idx = 1, 2
        elif max_idx == 1:  # ±Y面 → X, Z軸を使用
            u_idx, v_idx = 0, 2
        else:               # ±Z面 → X, Y軸を使用
            u_idx, v_idx = 0, 1

        # 各ループ（頂点）に対してUV座標を設定
        for loop_index in poly.loop_indices:
            loop = mesh.loops[loop_index]
            vert = mesh.vertices[loop.vertex_index]
            co = vert.co

            u = getattr(co, axes[u_idx])
            v = getattr(co, axes[v_idx])
            uv_layer.data[loop_index].uv = (u, v)

    # マテリアルを作成してテクスチャを設定
    material_name = f"ClayMaterial_{texture_file_name}"
    material = bpy.data.materials.get(material_name)
    if material is None:
        material = bpy.data.materials.new(name=material_name)
        material.use_nodes = True

        # プリンシプルBSDFを取得
        bsdf = material.node_tree.nodes.get("Principled BSDF")
        if bsdf is None:
            bsdf = material.node_tree.nodes.new("ShaderNodeBsdfPrincipled")

        # テクスチャノードを作成
        tex_node = material.node_tree.nodes.new("ShaderNodeTexImage")
        tex_node.name = "ClayTexture"
        tex_node.label = "ClayTexture"

        # テクスチャ画像を読み込み（既にロードされていれば再利用）
        try:
            image = bpy.data.images.load(texture_file_name, check_existing=True)
            tex_node.image = image
        except RuntimeError:
            print(f"WARNING: Failed to load texture image: {texture_file_name}")

        # テクスチャノードをBSDFのベースカラーに接続
        material.node_tree.links.new(
            bsdf.inputs["Base Color"],
            tex_node.outputs["Color"],
        )

    # メッシュにマテリアルを割り当て
    mesh.materials.clear()
    mesh.materials.append(material)


def fix_mtl_file(mtl_path, material_name, texture_relative_path):
    """
    Blenderが出力したMTLファイルを編集して、正しいテクスチャ参照を追加する。
    - map_Kd を正しい相対パスで追加
    - 不必要なテクスチャ参照を削除
    """
    with open(mtl_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    new_lines = []
    in_material = False
    for line in lines:
        stripped = line.strip()

        # マテリアル開始
        if stripped.startswith("newmtl "):
            # 直前のマテリアルの map_Kd を補完
            if in_material and not any(l.strip().startswith("map_Kd ") for l in new_lines[-20:]):
                new_lines.append(f"map_Kd {texture_relative_path}\n")
            in_material = True
            # マテリアル名を正しい名前に置換
            new_lines.append(f"newmtl {material_name}\n")
            continue

        # Blenderが書き込んだ map_Kd は絶対パスや違う名前になっているので置換
        if stripped.startswith("map_Kd "):
            new_lines.append(f"map_Kd {texture_relative_path}\n")
            continue

        # 不要な行（map_Kd以外のテクスチャ参照など）はそのまま
        new_lines.append(line)

    # 最後のマテリアルの map_Kd を補完
    if in_material and not any(l.strip().startswith("map_Kd ") for l in new_lines[-20:]):
        new_lines.append(f"map_Kd {texture_relative_path}\n")

    with open(mtl_path, "w", encoding="utf-8") as f:
        f.writelines(new_lines)


def export_obj(obj, output_dir, stageId, origin, color, texture_file_name):
    """OBJファイルとしてエクスポート（テクスチャ参照付きMTLも生成）"""
    output_path = Path(output_dir) / f"__clay_block_{stageId}_{origin}.obj"
    mtl_path = Path(output_dir) / f"__clay_block_{stageId}_{origin}.mtl"

    # マテリアル名（OBJ/MTLで共通）
    material_name = f"ClayMat_{origin}"

    # マテリアルをシンプルで短い名前に変更（MTLの整合性のため）
    mesh = obj.data
    if mesh.materials:
        mesh.materials[0].name = material_name

    # テクスチャ参照（OBJ ファイルから見て ../clay4.png の相対パス）
    texture_relative_path = f"../{texture_file_name}"

    # オブジェクトを選択
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj

    # エクスポート（法線とUVを含める。マテリアルはBlenderに書かせる）
    bpy.ops.wm.obj_export(
        filepath=str(output_path),
        export_selected_objects=True,
        export_animation=False,
        export_normals=True,   # 法線を含める
        export_uv=True,         # UV座標を含める
        export_materials=True,  # MTLをBlenderに生成させる
        apply_modifiers=True,   # モディファイアを適用した状態でエクスポート
    )

    # MTLファイルを編集して正しいテクスチャ参照に修正
    fix_mtl_file(mtl_path, material_name, texture_relative_path)

    return output_path


def resolve_texture_dir(stage_dir):
    """
    ステージディレクトリからテクスチャディレクトリの絶対パスを導出する。
    stage_dir = <root>/Game/Assets/csv/Map/StageXX
    → texture_dir = <root>/Game/Assets/Models/clay
    """
    stage_path = Path(stage_dir).resolve()
    parts = stage_path.parts
    # "Game" を含むパスを探して、そこを基準にする
    try:
        idx = len(parts) - 1 - list(reversed(parts)).index("Game")
    except ValueError:
        return None
    project_root = Path(*parts[:idx])
    return project_root / "Game" / "Assets" / "Models" / "clay"


def clean_output_dir(output_dir, stage_id):
    """
    出力ディレクトリから該当ステージの OBJ/MTL ファイルのみを削除する。
    他ステージのファイル（同じディレクトリに __clay_block_{他のstage}_*.obj が
    存在する場合もある）は削除しない。
    """
    if not output_dir.exists():
        return
    deleted_count = 0
    # 命名規則: __clay_block_{stage:02}_{origin}.obj / .mtl
    pattern = f"__clay_block_{stage_id}_*.obj"
    mtl_pattern = f"__clay_block_{stage_id}_*.mtl"
    for pat in (pattern, mtl_pattern):
        for old_file in output_dir.glob(pat):
            try:
                old_file.unlink()
                deleted_count += 1
            except OSError as e:
                print(f"WARNING: Failed to delete {old_file}: {e}")
    if deleted_count > 0:
        print(f"Cleaned {deleted_count} old stage {stage_id} files from {output_dir}")


def main():
    args = parse_args()
    print(f"Stage directory: {args.stage_dir}")
    print(f"Output directory: {args.output_dir}")
    print(f"Bevel radius: {args.bevel_radius}, segments: {args.segments}")

    # 出力ディレクトリを作成
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    # 既存のOBJ/MTLファイルを削除（古いモデルの取り残し防止）
    # 他ステージのファイルは削除しない（同一ディレクトリに複数ステージが共存する場合がある）
    clean_output_dir(output_dir, args.stage_id)

    # ステージディレクトリの存在確認
    stage_dir = Path(args.stage_dir)
    if not stage_dir.exists():
        print(f"ERROR: Stage directory does not exist: {stage_dir}")
        return
    print(f"Stage directory exists: {stage_dir}")

    # テクスチャディレクトリの絶対パスを解決
    texture_dir = resolve_texture_dir(args.stage_dir)
    if texture_dir is None:
        print(f"WARNING: Could not resolve texture directory from stage path. Using fallback.")
        texture_dir = Path(args.texture_base_dir)
    print(f"Texture directory: {texture_dir}")
    if not texture_dir.exists():
        print(f"WARNING: Texture directory does not exist: {texture_dir}")

    # データを読み込む
    layers = read_csv_layers(args.stage_dir)
    stage_json = read_stage_json(args.stage_dir)
    print(f"Loaded {len(layers)} layers")
    if layers:
        print(f"Layer 0 dimensions: {len(layers[0])}x{len(layers[0][0]) if layers[0] else 0}")

    if not layers:
        print("No clay data found")
        return

    # 次元を計算
    sizeY = len(layers)
    sizeZ = len(layers[0]) if layers else 0
    sizeX = len(layers[0][0]) if layers and layers[0] else 0

    # チップ配列を構築
    chips = []
    for y in range(sizeY):
        for z in range(sizeZ):
            for x in range(sizeX):
                chips.append(layers[y][z][x])

    # stage.json から clay cell の Color 番号を読み取る
    clayColorMap = read_clay_color_map(args.stage_dir, sizeX, sizeY, sizeZ)
    print(f"Loaded clay color map: {sum(1 for c in clayColorMap if c != 0)} colored cells")

    # 連結成分を計算
    clayOrigin = compute_connected_components(chips, sizeX, sizeY, sizeZ)

    # ボクセルメッシュを作成（色情報付き）
    mesh_objects = create_voxel_mesh(chips, clayOrigin, clayColorMap, sizeX, sizeY, sizeZ)

    # 各ブロックにベベルを適用してエクスポート
    for origin, obj, color in mesh_objects:
        texture_file_name = color_to_texture(color)
        # 絶対パスでテクスチャを読み込む
        texture_abs_path = texture_dir / texture_file_name
        if texture_abs_path.exists():
            texture_load_path = str(texture_abs_path)
        else:
            # フォールバック: 相対パス
            texture_load_path = str(texture_file_name)
            print(f"WARNING: Texture not found at {texture_abs_path}, using relative path")

        apply_bevel(obj, args.bevel_radius, args.segments)
        triangulate_mesh(obj)
        finalize_mesh(obj, texture_load_path)
        output_path = export_obj(obj, args.output_dir, args.stage_id, origin, color, texture_file_name)

        # オブジェクトを削除
        bpy.data.objects.remove(obj, do_unlink=True)

    print(f"Generated {len(mesh_objects)} clay block meshes")


if __name__ == "__main__":
    try:
        main()
        print("CLAY_MESH_GENERATION_SUCCESS")
    except Exception as e:
        import traceback
        print(f"CLAY_MESH_GENERATION_FAILED: {e}")
        traceback.print_exc()
        sys.exit(1)
