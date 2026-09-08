"""clay.obj の UV 島に arrow.png を焼き込み、伸長方向別の粘土テクスチャを生成する。

出力: clay*.png ごとに <stem>_{px,nx,pz,nz}.png(上面 + 伸長軸に平行な側面 2 面に矢印)
実行: このディレクトリで `python make_arrow_textures.py`
"""

from pathlib import Path

from PIL import Image

HERE = Path(__file__).parent
ARROW = HERE / "arrow.png"
COLORS = ["clay.png", "clay2.png", "clay3.png", "clay4.png", "clay5.png"]
# 伸長方向 → ファイル名の接尾辞
DIRECTIONS = {(1, 0, 0): "px", (-1, 0, 0): "nx", (0, 0, 1): "pz", (0, 0, -1): "nz"}
ARROW_FILL = 0.85


def load_obj(path):
    """面ごとの (world xyz, uv) 頂点列を法線方向 (axis, sign) で分類して返す"""
    v, vt, vn = [], [], []
    faces = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if not parts:
            continue
        # エンジン(PolygonMeshBuilder)は読み込み時に X を反転するので、世界座標に合わせて同じ反転をする
        if parts[0] == "v":
            x, y, z = map(float, parts[1:4])
            v.append((-x, y, z))
        elif parts[0] == "vt":
            vt.append(tuple(map(float, parts[1:3])))
        elif parts[0] == "vn":
            x, y, z = map(float, parts[1:4])
            vn.append((-x, y, z))
        elif parts[0] == "f":
            for token in parts[1:]:
                vi, ti, ni = (int(i) - 1 for i in token.split("/"))
                n = vn[ni]
                axis = max(range(3), key=lambda a: abs(n[a]))
                # 角丸部分を除いた平坦部だけを集める
                if abs(n[axis]) < 0.99:
                    continue
                key = (axis, 1 if n[axis] > 0 else -1)
                faces.setdefault(key, []).append((v[vi], vt[ti]))
    return faces


def uv_axis_of(verts, world_axis):
    """世界軸が UV のどの軸・向きに対応するかを相関で決める。戻り値 ("u"|"v", ±1)"""
    best = None
    for uv_index, name in ((0, "u"), (1, "v")):
        ws = [p[world_axis] for p, _ in verts]
        us = [t[uv_index] for _, t in verts]
        wm, um = sum(ws) / len(ws), sum(us) / len(us)
        cov = sum((w - wm) * (u - um) for w, u in zip(ws, us))
        if best is None or abs(cov) > abs(best[2]):
            best = (name, 1 if cov > 0 else -1, cov)
    return best[0], best[1]


def arrow_rotation(verts, direction):
    """direction(世界)が画像上で指す向きに合わせた arrow.png の回転角(反時計回り度)"""
    axis = next(a for a in range(3) if direction[a] != 0)
    uv_axis, sign = uv_axis_of(verts, axis)
    sign *= direction[axis]
    # 画素 y = (1 - v) * H なので +v は画像の上
    if uv_axis == "v":
        return 0 if sign > 0 else 180
    return -90 if sign > 0 else 90


def stamp(image, arrow, verts, direction):
    us = [t[0] for _, t in verts]
    vs = [t[1] for _, t in verts]
    w, h = image.size
    left, right = min(us) * w, max(us) * w
    top, bottom = (1 - max(vs)) * h, (1 - min(vs)) * h
    size = int(min(right - left, bottom - top) * ARROW_FILL)
    rotated = arrow.resize((size, size), Image.LANCZOS).rotate(arrow_rotation(verts, direction), expand=False)
    x = int((left + right) / 2 - size / 2)
    y = int((top + bottom) / 2 - size / 2)
    image.alpha_composite(rotated, (x, y))


def main():
    faces = load_obj(HERE / "clay.obj")
    arrow = Image.open(ARROW).convert("RGBA")
    for color in COLORS:
        base = Image.open(HERE / color).convert("RGBA")
        for direction, suffix in DIRECTIONS.items():
            image = base.copy()
            stretch_axis = 0 if direction[0] != 0 else 2
            side_axis = 2 - stretch_axis
            for face in ((1, 1), (side_axis, 1), (side_axis, -1)):
                stamp(image, arrow, faces[face], direction)
            out = HERE / f"{Path(color).stem}_{suffix}.png"
            image.save(out)
            print(out.name)


if __name__ == "__main__":
    main()
