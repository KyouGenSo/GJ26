"""
掴める対象の輪郭表示(反転ハル)用に、面の向きを反転した obj を生成する。

  python make_outline_meshes.py

clay/clay.obj → clay/clay_outline.obj (+ .mtl)、goalPiece/goalPiece.obj → goalPiece/goalPiece_outline.obj (+ .mtl)。
法線は符号反転、面は頂点順を逆にする。マテリアルは White.png 1 つにまとめる(色はコードで乗算)。
"""
import os

SOURCES = ["clay/clay.obj", "goalPiece/goalPiece.obj"]
HERE = os.path.dirname(os.path.abspath(__file__))


def convert(source: str) -> None:
    src_path = os.path.join(HERE, source)
    stem = os.path.splitext(os.path.basename(source))[0] + "_outline"
    out_dir = os.path.dirname(src_path)
    obj_path = os.path.join(out_dir, stem + ".obj")
    mtl_path = os.path.join(out_dir, stem + ".mtl")

    lines = [f"mtllib {stem}.mtl"]
    face_started = False
    lo = [float("inf")] * 3
    hi = [float("-inf")] * 3
    with open(src_path, encoding="utf-8", errors="ignore") as f:
        for raw in f:
            line = raw.rstrip("\n")
            parts = line.split()
            if not parts:
                continue
            tag = parts[0]
            if tag == "v":
                for i in range(3):
                    v = float(parts[1 + i])
                    lo[i] = min(lo[i], v)
                    hi[i] = max(hi[i], v)
                lines.append(line)
            elif tag == "vt":
                lines.append(line)
            elif tag == "vn":
                lines.append("vn " + " ".join(f"{-float(v):.4f}" for v in parts[1:4]))
            elif tag == "f":
                if not face_started:
                    lines.append("usemtl outline")
                    face_started = True
                lines.append("f " + " ".join(reversed(parts[1:])))
            # mtllib / usemtl / o / g / s は捨てる

    with open(obj_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    with open(mtl_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("newmtl outline\nKd 1.000000 1.000000 1.000000\nmap_Kd White.png\n")

    center = [(lo[i] + hi[i]) / 2 for i in range(3)]
    print(f"{source} -> {os.path.relpath(obj_path, HERE)}  bbox center = ({center[0]:.3f}, {center[1]:.3f}, {center[2]:.3f})")


if __name__ == "__main__":
    for source in SOURCES:
        convert(source)
