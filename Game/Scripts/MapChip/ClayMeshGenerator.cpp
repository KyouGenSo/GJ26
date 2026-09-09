#include "ClayMeshGenerator.h"

#include "MapChipField.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include <json.hpp>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/Json/JsonAsset.h>
#include <Engine/Assets/PolygonMesh/ProceduralMeshBuilder.h>
#include <Engine/GraphicsAPI/DirectX/DxResource/BufferObjects.h>
#include <Library/Math/Vector2.h>
#include <Library/Math/Vector3.h>

namespace {

constexpr i32 NUM_FACE_DIRS = 6;
constexpr i32 NUM_EDGE_DEFS = 12;
constexpr i32 NUM_CORNER_DEFS = 8;

const std::array<Vector3, NUM_FACE_DIRS> FACE_NORMALS{ {
	{ 1, 0, 0 },
	{ -1, 0, 0 },
	{ 0, 1, 0 },
	{ 0, -1, 0 },
	{ 0, 0, 1 },
	{ 0, 0, -1 },
} };

const std::array<MapChipIndex, NUM_FACE_DIRS> FACE_OFFSETS{ {
	{ 1, 0, 0 },
	{ -1, 0, 0 },
	{ 0, 1, 0 },
	{ 0, -1, 0 },
	{ 0, 0, 1 },
	{ 0, 0, -1 },
} };

struct EdgeDef {
	i32 faceA;
	i32 faceB;
};

const std::array<EdgeDef, NUM_EDGE_DEFS> EDGE_DEFS{ {
	{ 0, 2 }, { 0, 3 }, { 1, 2 }, { 1, 3 },
	{ 0, 4 }, { 0, 5 }, { 1, 4 }, { 1, 5 },
	{ 2, 4 }, { 2, 5 }, { 3, 4 }, { 3, 5 },
} };

struct CornerDef {
	i32 faceA;
	i32 faceB;
	i32 faceC;
};

const std::array<CornerDef, NUM_CORNER_DEFS> CORNER_DEFS{ {
	{ 0, 2, 4 }, { 0, 2, 5 }, { 0, 3, 4 }, { 0, 3, 5 },
	{ 1, 2, 4 }, { 1, 2, 5 }, { 1, 3, 4 }, { 1, 3, 5 },
} };

constexpr i32 faceAxis(i32 face) { return face / 2; }
constexpr i32 faceSign(i32 face) { return (face % 2 == 0) ? 1 : -1; }

Vector3 edgeCorner(i32 faceA, i32 faceB) {
	return (FACE_NORMALS[faceA] + FACE_NORMALS[faceB]) * 0.5f;
}

Vector3 cornerPosition(i32 faceA, i32 faceB, i32 faceC) {
	return (FACE_NORMALS[faceA] + FACE_NORMALS[faceB] + FACE_NORMALS[faceC]) * 0.5f;
}

i32 edgeAxisIndex(i32 faceA, i32 faceB) {
	return 3 - faceAxis(faceA) - faceAxis(faceB);
}

i32 find_corner_index(i32 fA, i32 fB, i32 fC) {
	const i32 minF = std::min(fA, std::min(fB, fC));
	const i32 maxF = std::max(fA, std::max(fB, fC));
	const i32 midF = fA + fB + fC - minF - maxF;
	for (i32 c = 0; c < NUM_CORNER_DEFS; ++c) {
		const i32 ca = CORNER_DEFS[c].faceA;
		const i32 cb = CORNER_DEFS[c].faceB;
		const i32 cc = CORNER_DEFS[c].faceC;
		const i32 minC = std::min(ca, std::min(cb, cc));
		const i32 maxC = std::max(ca, std::max(cb, cc));
		const i32 midC = ca + cb + cc - minC - maxC;
		if (minF == minC && midF == midC && maxF == maxC) {
			return c;
		}
	}
	return -1;
}

Vector3 cross(const Vector3& a, const Vector3& b) {
	return Vector3{
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x,
	};
}

r32 dot(const Vector3& a, const Vector3& b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

struct GenerationParams {
	r32 bevelRadius = 0.15f;
	i32 segment = 3;
};

GenerationParams load_params() {
	GenerationParams params;
	szg::JsonAsset param{ "[[game]]/ClayGenerationParam.param" };
	const nlohmann::json& json = param.cget();
	if (!json.is_object()) {
		return params;
	}
	const auto readI32 = [&json](const char* name, i32 fallback) {
		return json.value(name, nlohmann::json::object()).value("value", fallback);
	};
	const auto readR32 = [&json](const char* name, r32 fallback) {
		return json.value(name, nlohmann::json::object()).value("value", fallback);
	};
	params.segment = std::max(readI32("Segment", params.segment), 1);
	params.bevelRadius = std::max(readR32("BevelRadius", params.bevelRadius), 0.01f);
	params.bevelRadius = std::min(params.bevelRadius, 0.49f);
	return params;
}

enum class EdgeType { None, Convex, Concave };
enum class CornerType { None, Convex, Concave };

struct CellContext {
	i32 cx, cy, cz;
	std::array<bool, NUM_FACE_DIRS> faceExposed{};
	std::array<EdgeType, NUM_EDGE_DEFS> edgeType{};
	std::array<CornerType, NUM_CORNER_DEFS> cornerType{};
};

struct BlockData {
	const std::vector<MapChipType>* chips;
	i32 sizeX, sizeY, sizeZ;

	i32 flatIndex(i32 x, i32 y, i32 z) const {
		return x + sizeX * (z + sizeZ * y);
	}

	bool isInside(i32 x, i32 y, i32 z) const {
		return 0 <= x && x < sizeX && 0 <= y && y < sizeY && 0 <= z && z < sizeZ;
	}

	bool isClay(i32 x, i32 y, i32 z) const {
		if (!isInside(x, y, z)) {
			return false;
		}
		const i32 flat = flatIndex(x, y, z);
		return (*chips)[flat] == MapChipType::Clay;
	}
};

CellContext classify_cell(const BlockData& block, i32 cx, i32 cy, i32 cz) {
	CellContext ctx{ cx, cy, cz };

	for (i32 f = 0; f < NUM_FACE_DIRS; ++f) {
		const MapChipIndex& off = FACE_OFFSETS[f];
		ctx.faceExposed[f] = !block.isClay(cx + off.x, cy + off.y, cz + off.z);
	}

	for (i32 e = 0; e < NUM_EDGE_DEFS; ++e) {
		const i32 fA = EDGE_DEFS[e].faceA;
		const i32 fB = EDGE_DEFS[e].faceB;
		const MapChipIndex& offA = FACE_OFFSETS[fA];
		const MapChipIndex& offB = FACE_OFFSETS[fB];
		const bool nA = block.isClay(cx + offA.x, cy + offA.y, cz + offA.z);
		const bool nB = block.isClay(cx + offB.x, cy + offB.y, cz + offB.z);
		const bool nAB = block.isClay(cx + offA.x + offB.x, cy + offA.y + offB.y, cz + offA.z + offB.z);

		if (!nA && !nB) {
			ctx.edgeType[e] = EdgeType::Convex;
		} else if ((nA && !nB && nAB) || (!nA && nB && nAB)) {
			ctx.edgeType[e] = EdgeType::Concave;
		} else {
			ctx.edgeType[e] = EdgeType::None;
		}
	}

	for (i32 c = 0; c < NUM_CORNER_DEFS; ++c) {
		const i32 fA = CORNER_DEFS[c].faceA;
		const i32 fB = CORNER_DEFS[c].faceB;
		const i32 fC = CORNER_DEFS[c].faceC;
		const MapChipIndex& offA = FACE_OFFSETS[fA];
		const MapChipIndex& offB = FACE_OFFSETS[fB];
		const MapChipIndex& offC = FACE_OFFSETS[fC];

		const bool nA = block.isClay(cx + offA.x, cy + offA.y, cz + offA.z);
		const bool nB = block.isClay(cx + offB.x, cy + offB.y, cz + offB.z);
		const bool nC = block.isClay(cx + offC.x, cy + offC.y, cz + offC.z);
		const bool nAB = block.isClay(cx + offA.x + offB.x, cy + offA.y + offB.y, cz + offA.z + offB.z);
		const bool nAC = block.isClay(cx + offA.x + offC.x, cy + offA.y + offC.y, cz + offA.z + offC.z);
		const bool nBC = block.isClay(cx + offB.x + offC.x, cy + offB.y + offC.y, cz + offB.z + offC.z);
		const bool nABC = block.isClay(
			cx + offA.x + offB.x + offC.x,
			cy + offA.y + offB.y + offC.y,
			cz + offA.z + offB.z + offC.z);

		if (!nA && !nB && !nC && !nAB && !nAC && !nBC) {
			ctx.cornerType[c] = CornerType::Convex;
		} else if (nA && nB && nC && nAB && nAC && nBC && !nABC) {
			ctx.cornerType[c] = CornerType::Concave;
		} else {
			ctx.cornerType[c] = CornerType::None;
		}
	}

	return ctx;
}

void get_face_insets(
	const CellContext& ctx,
	i32 face,
	r32 bevelRadius,
	r32& uMin, r32& uMax,
	r32& vMin, r32& vMax) {

	const i32 axis = faceAxis(face);
	i32 uAxis = -1, vAxis = -1;
	for (i32 a = 0; a < 3; ++a) {
		if (a != axis) {
			if (uAxis < 0) uAxis = a;
			else vAxis = a;
		}
	}

	uMin = -0.5f;
	uMax = 0.5f;
	vMin = -0.5f;
	vMax = 0.5f;

	for (i32 e = 0; e < NUM_EDGE_DEFS; ++e) {
		if (ctx.edgeType[e] == EdgeType::None) {
			continue;
		}
		const i32 fA = EDGE_DEFS[e].faceA;
		const i32 fB = EDGE_DEFS[e].faceB;
		if (fA != face && fB != face) {
			continue;
		}
		const i32 otherFace = (fA == face) ? fB : fA;
		const i32 otherAxis = faceAxis(otherFace);
		const i32 otherSign = faceSign(otherFace);

		if (otherAxis == uAxis) {
			if (otherSign > 0) uMax -= bevelRadius;
			else uMin += bevelRadius;
		} else if (otherAxis == vAxis) {
			if (otherSign > 0) vMax -= bevelRadius;
			else vMin += bevelRadius;
		}
	}
}

void generate_face(
	std::vector<szg::VertexDataBuffer>& vertices,
	std::vector<u32>& indices,
	const CellContext& ctx,
	i32 face,
	const Vector3& cellOffset,
	r32 bevelRadius) {

	const Vector3& normal = FACE_NORMALS[face];
	const i32 axis = faceAxis(face);

	i32 uAxis = -1, vAxis = -1;
	for (i32 a = 0; a < 3; ++a) {
		if (a != axis) {
			if (uAxis < 0) uAxis = a;
			else vAxis = a;
		}
	}

	r32 uMin, uMax, vMin, vMax;
	get_face_insets(ctx, face, bevelRadius, uMin, uMax, vMin, vMax);

	Vector3 base = cellOffset + normal * 0.5f;

	Vector3 uDir{};
	Vector3 vDir{};
	switch (uAxis) {
	case 0: uDir.x = 1.0f; break;
	case 1: uDir.y = 1.0f; break;
	case 2: uDir.z = 1.0f; break;
	}
	switch (vAxis) {
	case 0: vDir.x = 1.0f; break;
	case 1: vDir.y = 1.0f; break;
	case 2: vDir.z = 1.0f; break;
	}

	const u32 baseIdx = static_cast<u32>(vertices.size());

	vertices.push_back({ base + uDir * uMin + vDir * vMin, Vector2{ 0, 0 }, normal });
	vertices.push_back({ base + uDir * uMax + vDir * vMin, Vector2{ 1, 0 }, normal });
	vertices.push_back({ base + uDir * uMax + vDir * vMax, Vector2{ 1, 1 }, normal });
	vertices.push_back({ base + uDir * uMin + vDir * vMax, Vector2{ 0, 1 }, normal });

	const r32 windingSign = dot(cross(uDir, vDir), normal);
	if (windingSign >= 0.0f) {
		indices.push_back(baseIdx + 0);
		indices.push_back(baseIdx + 1);
		indices.push_back(baseIdx + 2);
		indices.push_back(baseIdx + 0);
		indices.push_back(baseIdx + 2);
		indices.push_back(baseIdx + 3);
	} else {
		indices.push_back(baseIdx + 0);
		indices.push_back(baseIdx + 2);
		indices.push_back(baseIdx + 1);
		indices.push_back(baseIdx + 0);
		indices.push_back(baseIdx + 3);
		indices.push_back(baseIdx + 2);
	}
}

void generate_edge_fillet(
	std::vector<szg::VertexDataBuffer>& vertices,
	std::vector<u32>& indices,
	const Vector3& cellOffset,
	i32 faceA, i32 faceB,
	EdgeType type,
	r32 bevelRadius, i32 segment,
	r32 startOffset, r32 endOffset) {

	const Vector3& nA = FACE_NORMALS[faceA];
	const Vector3& nB = FACE_NORMALS[faceB];
	const Vector3 edge = edgeCorner(faceA, faceB);
	const i32 axisIdx = edgeAxisIndex(faceA, faceB);

	Vector3 axisDir{};
	switch (axisIdx) {
	case 0: axisDir.x = 1.0f; break;
	case 1: axisDir.y = 1.0f; break;
	case 2: axisDir.z = 1.0f; break;
	}

	const Vector3 center = edge - (nA + nB) * bevelRadius;

	const r32 angleStart = 0.0f;
	const r32 angleEnd = static_cast<r32>(std::numbers::pi / 2.0);

	const u32 baseIdx = static_cast<u32>(vertices.size());

	for (i32 s = 0; s <= segment; ++s) {
		const r32 t = static_cast<r32>(s) / static_cast<r32>(segment);
		const r32 angle = angleStart + t * (angleEnd - angleStart);
		const r32 cosA = std::cos(angle);
		const r32 sinA = std::sin(angle);
		const Vector3 dir = nA * cosA + nB * sinA;

		const Vector3 pos = center + dir * bevelRadius;
		const Vector3 normal = dir;

		vertices.push_back({ cellOffset + pos + axisDir * startOffset, Vector2{ t, 0 }, normal });
		vertices.push_back({ cellOffset + pos + axisDir * endOffset, Vector2{ t, 1 }, normal });
	}

	const r32 edgeSign = dot(cross(nB, axisDir), nA);
	const bool flip = edgeSign < 0.0f;

	for (i32 s = 0; s < segment; ++s) {
		const u32 i0 = baseIdx + static_cast<u32>(s * 2);
		const u32 i1 = i0 + 1;
		const u32 i2 = i0 + 2;
		const u32 i3 = i0 + 3;

		if (!flip) {
			indices.push_back(i0);
			indices.push_back(i2);
			indices.push_back(i1);
			indices.push_back(i1);
			indices.push_back(i2);
			indices.push_back(i3);
		} else {
			indices.push_back(i0);
			indices.push_back(i1);
			indices.push_back(i2);
			indices.push_back(i1);
			indices.push_back(i3);
			indices.push_back(i2);
		}
	}
}

void generate_corner_fillet(
	std::vector<szg::VertexDataBuffer>& vertices,
	std::vector<u32>& indices,
	const Vector3& cellOffset,
	i32 faceA, i32 faceB, i32 faceC,
	CornerType type,
	r32 bevelRadius, i32 segment) {

	const Vector3& nA = FACE_NORMALS[faceA];
	const Vector3& nB = FACE_NORMALS[faceB];
	const Vector3& nC = FACE_NORMALS[faceC];
	const Vector3 corner = cornerPosition(faceA, faceB, faceC);

	const Vector3 center = corner - (nA + nB + nC) * bevelRadius;

	const u32 baseIdx = static_cast<u32>(vertices.size());

	for (i32 j = 0; j <= segment; ++j) {
		const r32 phi = static_cast<r32>(j) / static_cast<r32>(segment) * static_cast<r32>(std::numbers::pi / 2.0);
		const r32 cp = std::cos(phi);
		const r32 sp = std::sin(phi);
		const bool isSingular = (j == segment);
		const i32 iMax = isSingular ? 0 : segment;
		for (i32 i = 0; i <= iMax; ++i) {
			const r32 theta = static_cast<r32>(i) / static_cast<r32>(segment) * static_cast<r32>(std::numbers::pi / 2.0);
			const r32 ct = std::cos(theta);
			const r32 st = std::sin(theta);
			Vector3 dir;
			if (isSingular) {
				dir = nC;
			} else {
				dir = nA * (cp * ct) + nB * (cp * st) + nC * sp;
			}

		const Vector3 pos = center + dir * bevelRadius;
		const Vector3 normal = dir;

			const r32 u = isSingular ? 0.5f : static_cast<r32>(i) / static_cast<r32>(segment);
			const r32 v = static_cast<r32>(j) / static_cast<r32>(segment);
			vertices.push_back({ cellOffset + pos, Vector2{ u, v }, normal });
		}
	}

	const r32 cornerSign = dot(cross(nB, nC), nA);
	const bool flip = cornerSign > 0.0f;

	std::vector<u32> rowStartIndices(segment + 1);
	u32 currentIdx = baseIdx;
	for (i32 j = 0; j <= segment; ++j) {
		rowStartIndices[j] = currentIdx;
		if (j < segment) {
			currentIdx += segment + 1;
		} else {
			currentIdx += 1;
		}
	}

	for (i32 j = 0; j < segment; ++j) {
		const bool isLastRow = (j == segment - 1);

		for (i32 i = 0; i < segment; ++i) {
			const u32 i0 = rowStartIndices[j] + i;
			const u32 i1 = i0 + 1;

			if (isLastRow) {
				const u32 iSingular = rowStartIndices[j + 1];
				if (!flip) {
					indices.push_back(i0);
					indices.push_back(iSingular);
					indices.push_back(i1);
				} else {
					indices.push_back(i0);
					indices.push_back(i1);
					indices.push_back(iSingular);
				}
			} else {
				const u32 i2 = rowStartIndices[j + 1] + i;
				const u32 i3 = i2 + 1;

				if (!flip) {
					indices.push_back(i0);
					indices.push_back(i2);
					indices.push_back(i1);
					indices.push_back(i1);
					indices.push_back(i2);
					indices.push_back(i3);
				} else {
					indices.push_back(i0);
					indices.push_back(i1);
					indices.push_back(i2);
					indices.push_back(i1);
					indices.push_back(i3);
					indices.push_back(i2);
				}
			}
		}
	}
}

} // namespace

void ClayMeshGenerator::Generate(i32 stageNumber, i32 originFlat, const MapChipField& field, u8 clayColor) {
	const GenerationParams params = load_params();
	const auto cellsData = field.cells();

	BlockData block{
		&cellsData.chips,
		field.width(), field.height(), field.depth(),
	};

	const i32 totalCells = static_cast<i32>(cellsData.chips.size());
	const i32 originX = originFlat % field.width();
	const i32 originZ = (originFlat / field.width()) % field.depth();
	const i32 originY = originFlat / (field.width() * field.depth());

	std::vector<szg::VertexDataBuffer> vertices;
	std::vector<u32> indices;

	for (i32 i = 0; i < totalCells; ++i) {
		if (cellsData.chips[i] != MapChipType::Clay || cellsData.clayOrigin[i] != originFlat) {
			continue;
		}
		const i32 cx = i % field.width();
		const i32 cz = (i / field.width()) % field.depth();
		const i32 cy = i / (field.width() * field.depth());

		const Vector3 cellOffset{
			static_cast<r32>(cx - originX),
			static_cast<r32>(cy - originY),
			static_cast<r32>(cz - originZ),
		};

		const CellContext ctx = classify_cell(block, cx, cy, cz);

		for (i32 f = 0; f < NUM_FACE_DIRS; ++f) {
			if (ctx.faceExposed[f]) {
				generate_face(vertices, indices, ctx, f, cellOffset, params.bevelRadius);
			}
		}

		for (i32 e = 0; e < NUM_EDGE_DEFS; ++e) {
			if (ctx.edgeType[e] == EdgeType::None) {
				continue;
			}
			const i32 fA = EDGE_DEFS[e].faceA;
			const i32 fB = EDGE_DEFS[e].faceB;
			const i32 axisIdx = edgeAxisIndex(fA, fB);
			const i32 negFace = 2 * axisIdx + 1;
			const i32 posFace = 2 * axisIdx;
			const i32 negCorner = find_corner_index(negFace, fA, fB);
			const i32 posCorner = find_corner_index(posFace, fA, fB);

			r32 startOffset = -0.5f;
			r32 endOffset = 0.5f;
			if (negCorner >= 0 && ctx.cornerType[negCorner] != CornerType::None) {
				startOffset += params.bevelRadius;
			}
			if (posCorner >= 0 && ctx.cornerType[posCorner] != CornerType::None) {
				endOffset -= params.bevelRadius;
			}

			generate_edge_fillet(
				vertices, indices, cellOffset,
				fA, fB,
				ctx.edgeType[e],
				params.bevelRadius, params.segment,
				startOffset, endOffset);
		}

		for (i32 c = 0; c < NUM_CORNER_DEFS; ++c) {
			if (ctx.cornerType[c] == CornerType::None) {
				continue;
			}
			const i32 fA = CORNER_DEFS[c].faceA;
			const i32 fB = CORNER_DEFS[c].faceB;
			const i32 fC = CORNER_DEFS[c].faceC;
			generate_corner_fillet(
				vertices, indices, cellOffset,
				fA, fB, fC,
				ctx.cornerType[c],
				params.bevelRadius, params.segment);
		}
	}

	if (vertices.empty() || indices.empty()) {
		return;
	}

	const std::string meshName = MeshName(stageNumber, originFlat);
	const std::string textureName = (0 < clayColor && clayColor < ClayColor::Count)
		? ClayColor::Textures[clayColor]
		: "clay.png";
	szg::ProceduralMeshBuilder builder;
	builder.add_submesh(std::move(vertices), std::move(indices), "clayMaterial")
		.set_material("clayMaterial", textureName);
	builder.build_and_register(meshName);
}

void ClayMeshGenerator::Remove(const std::string& meshName) {
	szg::ProceduralMeshBuilder::unregister(meshName);
}
