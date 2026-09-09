#include "ClayStretchMeshGenerator.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include <json.hpp>

#include <Engine/Assets/Json/JsonAsset.h>
#include <Engine/Assets/PolygonMesh/ProceduralMeshBuilder.h>
#include <Engine/GraphicsAPI/DirectX/DxResource/BufferObjects.h>
#include <Library/Math/Vector2.h>

namespace {

constexpr std::array<Vector3, 4> STRETCH_DIRS{ {
	{ 1, 0, 0 },
	{ -1, 0, 0 },
	{ 0, 0, 1 },
	{ 0, 0, -1 },
} };

i32 stretch_dir_index(const Vector3& dir) {
	for (i32 i = 0; i < 4; ++i) {
		if (static_cast<i32>(STRETCH_DIRS[i].x) == static_cast<i32>(dir.x) &&
			static_cast<i32>(STRETCH_DIRS[i].z) == static_cast<i32>(dir.z)) {
			return i;
		}
	}
	return 0;
}

struct RingProfile {
	r32 halfW;
	r32 halfD;
	r32 cornerR;
};

r32 smoothstep(r32 edge0, r32 edge1, r32 x) {
	const r32 t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

RingProfile compute_ring_profile(
	r32 axisT,
	r32 progress,
	const ClayStretchParams& params,
	r32 bevelRadius) {

	const r32 currentNeckDepth = params.neckDepth * (1.0f - smoothstep(params.neckResolveStart, 1.0f, progress));
	const r32 neckFactor = 1.0f - currentNeckDepth * std::sin(axisT * static_cast<r32>(std::numbers::pi));

	r32 baseHalfW = 0.5f;
	r32 baseHalfD = 0.5f;
	r32 baseCornerR = bevelRadius;

	r32 tipHalfW, tipHalfD, tipCornerR;

	if (progress < params.tipDomeThreshold) {
		const r32 domeT = progress / params.tipDomeThreshold;
		tipHalfW = baseHalfW * 0.3f * domeT;
		tipHalfD = baseHalfD * 0.3f * domeT;
		tipCornerR = std::min(tipHalfW, tipHalfD) * 0.5f;
	} else {
		const r32 flatT = (progress - params.tipDomeThreshold) / (1.0f - params.tipDomeThreshold);
		tipHalfW = baseHalfW;
		tipHalfD = baseHalfD;
		tipCornerR = bevelRadius * flatT;
	}

	RingProfile profile;
	profile.halfW = baseHalfW + (tipHalfW - baseHalfW) * axisT;
	profile.halfD = baseHalfD + (tipHalfD - baseHalfD) * axisT;
	profile.cornerR = baseCornerR + (tipCornerR - baseCornerR) * axisT;

	profile.halfW *= neckFactor;
	profile.halfD *= neckFactor;
	profile.cornerR *= neckFactor;

	profile.cornerR = std::min(profile.cornerR, std::min(profile.halfW, profile.halfD) * 0.99f);

	return profile;
}

struct RingPoint {
	Vector3 position;
	Vector3 normal;
};

RingPoint compute_ring_point(
	r32 axisPos,
	r32 segmentT,
	const RingProfile& profile,
	const Vector3& axisDir,
	const Vector3& uDir,
	const Vector3& vDir) {

	const r32 perimeterSegments = static_cast<r32>(4);
	const r32 cornerSegments = std::max(1.0f, static_cast<r32>((profile.cornerR > 0.001f) ? 4 : 1));
	const r32 totalSegments = perimeterSegments + (profile.cornerR > 0.001f ? perimeterSegments * (cornerSegments - 1) : 0);
	const r32 totalT = segmentT * totalSegments;

	r32 localX, localZ;

	if (profile.cornerR > 0.001f) {
		const r32 innerW = profile.halfW - profile.cornerR;
		const r32 innerD = profile.halfD - profile.cornerR;

		const r32 straightLen = 2.0f * (innerW + innerD);
		const r32 cornerArcLen = 2.0f * static_cast<r32>(std::numbers::pi) * profile.cornerR;
		const r32 totalLen = straightLen + cornerArcLen;

		r32 dist = segmentT * totalLen;

		const r32 side1 = innerW;
		const r32 corner1 = static_cast<r32>(std::numbers::pi) * profile.cornerR * 0.5f;
		const r32 side2 = innerD * 2.0f;
		const r32 corner2 = corner1;
		const r32 side3 = innerW * 2.0f;
		const r32 corner3 = corner1;
		const r32 side4 = innerD * 2.0f;
		const r32 corner4 = corner1;

		if (dist < side1) {
			localX = innerW;
			localZ = -innerD + dist;
		} else if (dist < side1 + corner1) {
			const r32 angle = (dist - side1) / profile.cornerR;
			localX = innerW - profile.cornerR * (1.0f - std::cos(angle));
			localZ = innerD - profile.cornerR + profile.cornerR * std::sin(angle);
		} else if (dist < side1 + corner1 + side2) {
			const r32 d = dist - side1 - corner1;
			localX = innerW - d;
			localZ = innerD;
		} else if (dist < side1 + corner1 + side2 + corner2) {
			const r32 d = dist - side1 - corner1 - side2;
			const r32 angle = d / profile.cornerR;
			localX = -innerW + profile.cornerR - profile.cornerR * std::sin(angle);
			localZ = innerD - profile.cornerR * (1.0f - std::cos(angle));
		} else if (dist < side1 + corner1 + side2 + corner2 + side3) {
			const r32 d = dist - side1 - corner1 - side2 - corner2;
			localX = -innerW;
			localZ = innerD - d;
		} else if (dist < side1 + corner1 + side2 + corner2 + side3 + corner3) {
			const r32 d = dist - side1 - corner1 - side2 - corner2 - side3;
			const r32 angle = d / profile.cornerR;
			localX = -innerW + profile.cornerR * (1.0f - std::cos(angle));
			localZ = -innerD + profile.cornerR - profile.cornerR * std::sin(angle);
		} else if (dist < side1 + corner1 + side2 + corner2 + side3 + corner3 + side4) {
			const r32 d = dist - side1 - corner1 - side2 - corner2 - side3 - corner3;
			localX = -innerW + d;
			localZ = -innerD;
		} else {
			const r32 d = dist - side1 - corner1 - side2 - corner2 - side3 - corner3 - side4;
			const r32 angle = d / profile.cornerR;
			localX = innerW - profile.cornerR + profile.cornerR * std::sin(angle);
			localZ = -innerD + profile.cornerR * (1.0f - std::cos(angle));
		}
	} else {
		const r32 perimeter = 2.0f * (profile.halfW + profile.halfD);
		r32 dist = segmentT * perimeter;

		if (dist < profile.halfW) {
			localX = profile.halfW;
			localZ = -profile.halfD + dist;
		} else if (dist < profile.halfW + profile.halfD) {
			localX = profile.halfW - (dist - profile.halfW);
			localZ = profile.halfD;
		} else if (dist < profile.halfW * 2.0f + profile.halfD) {
			localX = -profile.halfW;
			localZ = profile.halfD - (dist - profile.halfW - profile.halfD);
		} else {
			localX = -profile.halfW + (dist - profile.halfW * 2.0f - profile.halfD);
			localZ = -profile.halfD;
		}
	}

	const Vector3 localPos = uDir * localX + vDir * localZ;
	const Vector3 worldPos = axisDir * axisPos + localPos;

	Vector3 normal = uDir * localX + vDir * localZ;
	const r32 normalLen = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
	if (normalLen > 0.001f) {
		normal = normal * (1.0f / normalLen);
	} else {
		normal = uDir;
	}

	return RingPoint{ worldPos, normal };
}

void generate_tip_cap(
	std::vector<szg::VertexDataBuffer>& vertices,
	std::vector<u32>& indices,
	r32 progress,
	const ClayStretchParams& params,
	r32 bevelRadius,
	i32 bevelSegment,
	const Vector3& axisDir,
	const Vector3& uDir,
	const Vector3& vDir) {

	const RingProfile tipProfile = compute_ring_profile(1.0f, progress, params, bevelRadius);
	const r32 tipAxisPos = progress;

	if (progress < params.tipDomeThreshold) {
		const r32 domeT = progress / params.tipDomeThreshold;
		const r32 domeRadius = std::max(tipProfile.halfW, tipProfile.halfD);

		const u32 baseIdx = static_cast<u32>(vertices.size());
		const i32 latSegments = std::max(4, params.cornerSegments * 2);
		const i32 lonSegments = std::max(4, params.cornerSegments * 2);

		vertices.push_back({
			axisDir * tipAxisPos,
			Vector2{ 0.5f, 0.0f },
			axisDir
		});

		for (i32 lat = 1; lat <= latSegments; ++lat) {
			const r32 phi = (static_cast<r32>(lat) / latSegments) * static_cast<r32>(std::numbers::pi) * 0.5f;
			const r32 ringRadius = domeRadius * std::cos(phi) * domeT;
			const r32 ringAxisOffset = domeRadius * std::sin(phi) * domeT;

			for (i32 lon = 0; lon <= lonSegments; ++lon) {
				const r32 theta = (static_cast<r32>(lon) / lonSegments) * 2.0f * static_cast<r32>(std::numbers::pi);
				const r32 lx = ringRadius * std::cos(theta);
				const r32 lz = ringRadius * std::sin(theta);

				const Vector3 pos = axisDir * (tipAxisPos + ringAxisOffset) + uDir * lx + vDir * lz;
				Vector3 normal = pos - axisDir * tipAxisPos;
				const r32 nLen = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
				if (nLen > 0.001f) {
					normal = normal * (1.0f / nLen);
				}

				vertices.push_back({
					pos,
					Vector2{ static_cast<r32>(lon) / lonSegments, static_cast<r32>(lat) / latSegments },
					normal
				});
			}
		}

		for (i32 lon = 0; lon < lonSegments; ++lon) {
			indices.push_back(baseIdx);
			indices.push_back(baseIdx + 1 + lon);
			indices.push_back(baseIdx + 1 + lon + 1);
		}

		for (i32 lat = 1; lat < latSegments; ++lat) {
			const u32 ringStart = baseIdx + 1 + (lat - 1) * (lonSegments + 1);
			const u32 nextRingStart = baseIdx + 1 + lat * (lonSegments + 1);

			for (i32 lon = 0; lon < lonSegments; ++lon) {
				indices.push_back(ringStart + lon);
				indices.push_back(nextRingStart + lon);
				indices.push_back(ringStart + lon + 1);

				indices.push_back(ringStart + lon + 1);
				indices.push_back(nextRingStart + lon);
				indices.push_back(nextRingStart + lon + 1);
			}
		}
	} else {
		const r32 flatT = (progress - params.tipDomeThreshold) / (1.0f - params.tipDomeThreshold);
		const r32 capBevelRadius = bevelRadius * flatT;

		if (capBevelRadius > 0.001f && bevelSegment > 0) {
			const u32 baseIdx = static_cast<u32>(vertices.size());
			const i32 cornerVerts = params.cornerSegments + 1;
			const i32 ringVerts = cornerVerts * 4;

			for (i32 s = 0; s <= params.cornerSegments; ++s) {
				const r32 angle = (static_cast<r32>(s) / params.cornerSegments) * static_cast<r32>(std::numbers::pi) * 0.5f;
				const r32 cosA = std::cos(angle);
				const r32 sinA = std::sin(angle);

				const r32 offsetX = capBevelRadius * (1.0f - cosA);
				const r32 offsetZ = capBevelRadius * (1.0f - sinA);
				const r32 normalX = cosA;
				const r32 normalZ = sinA;

				const Vector3 pos = axisDir * tipAxisPos + uDir * (tipProfile.halfW - offsetX) + vDir * (tipProfile.halfD - offsetZ);
				const Vector3 normal = uDir * normalX + vDir * normalZ;

				vertices.push_back({ pos, Vector2{ 0.0f, 0.0f }, normal });
			}

			for (i32 corner = 0; corner < 4; ++corner) {
				const r32 signU = (corner < 2) ? 1.0f : -1.0f;
				const r32 signV = (corner % 2 == 0) ? 1.0f : -1.0f;

				for (i32 s = 0; s <= params.cornerSegments; ++s) {
					const u32 srcIdx = baseIdx + static_cast<u32>(s);
					const szg::VertexDataBuffer& srcVert = vertices[srcIdx];

					Vector3 pos = axisDir * tipAxisPos;
					pos = pos + uDir * (srcVert.position.x - axisDir.x * tipAxisPos) * signU;
					pos = pos + vDir * (srcVert.position.z - axisDir.z * tipAxisPos) * signV;

					Vector3 normal = uDir * srcVert.normal.x * signU + vDir * srcVert.normal.z * signV;

					vertices.push_back({ pos, Vector2{ 0.0f, 0.0f }, normal });
				}
			}

			const u32 centerIdx = static_cast<u32>(vertices.size());
			vertices.push_back({
				axisDir * tipAxisPos,
				Vector2{ 0.5f, 0.5f },
				axisDir
			});

			for (i32 corner = 0; corner < 4; ++corner) {
				const u32 cornerStart = baseIdx + static_cast<u32>(corner * (params.cornerSegments + 1));
				const u32 nextCornerStart = baseIdx + static_cast<u32>(((corner + 1) % 4) * (params.cornerSegments + 1));

				for (i32 s = 0; s < params.cornerSegments; ++s) {
					indices.push_back(centerIdx);
					indices.push_back(cornerStart + static_cast<u32>(s));
					indices.push_back(cornerStart + static_cast<u32>(s) + 1);
				}

				indices.push_back(centerIdx);
				indices.push_back(cornerStart + static_cast<u32>(params.cornerSegments));
				indices.push_back(nextCornerStart);
			}
		} else {
			const u32 baseIdx = static_cast<u32>(vertices.size());
			const i32 ringVerts = 4;

			vertices.push_back({ axisDir * tipAxisPos + uDir * tipProfile.halfW + vDir * tipProfile.halfD, Vector2{ 0, 0 }, axisDir });
			vertices.push_back({ axisDir * tipAxisPos + uDir * tipProfile.halfW - vDir * tipProfile.halfD, Vector2{ 1, 0 }, axisDir });
			vertices.push_back({ axisDir * tipAxisPos - uDir * tipProfile.halfW - vDir * tipProfile.halfD, Vector2{ 1, 1 }, axisDir });
			vertices.push_back({ axisDir * tipAxisPos - uDir * tipProfile.halfW + vDir * tipProfile.halfD, Vector2{ 0, 1 }, axisDir });

			indices.push_back(baseIdx + 0);
			indices.push_back(baseIdx + 1);
			indices.push_back(baseIdx + 2);
			indices.push_back(baseIdx + 0);
			indices.push_back(baseIdx + 2);
			indices.push_back(baseIdx + 3);
		}
	}
}

void generate_single_frame(
	const std::string& meshName,
	const Vector3& direction,
	r32 progress,
	const ClayStretchParams& params,
	r32 bevelRadius,
	i32 bevelSegment) {

	if (progress <= 0.001f) {
		return;
	}

	const i32 dirIdx = stretch_dir_index(direction);
	const Vector3 axisDir = STRETCH_DIRS[dirIdx];

	Vector3 uDir, vDir;
	if (std::abs(axisDir.x) > 0.5f) {
		uDir = { 0, 1, 0 };
		vDir = { 0, 0, 1 };
	} else {
		uDir = { 1, 0, 0 };
		vDir = { 0, 1, 0 };
	}

	std::vector<szg::VertexDataBuffer> vertices;
	std::vector<u32> indices;

	const i32 ringCount = params.ringCount;
	const i32 cornerSegments = params.cornerSegments;
	const i32 ringVertexCount = (cornerSegments + 1) * 4;

	for (i32 r = 0; r <= ringCount; ++r) {
		const r32 axisT = static_cast<r32>(r) / static_cast<r32>(ringCount);
		const r32 axisPos = axisT * progress;

		const RingProfile profile = compute_ring_profile(axisT, progress, params, bevelRadius);

		const u32 ringBaseIdx = static_cast<u32>(vertices.size());

		const i32 totalSegments = (cornerSegments + 1) * 4;
		for (i32 s = 0; s <= totalSegments; ++s) {
			const r32 segmentT = static_cast<r32>(s % totalSegments) / static_cast<r32>(totalSegments);

			RingPoint point = compute_ring_point(axisPos, segmentT, profile, axisDir, uDir, vDir);

			vertices.push_back({
				point.position,
				Vector2{ segmentT, axisT },
				point.normal
			});
		}

		if (r > 0) {
			const u32 prevRingBase = ringBaseIdx - static_cast<u32>(ringVertexCount + 1);
			const u32 currRingBase = ringBaseIdx;

			for (i32 s = 0; s < totalSegments; ++s) {
				const u32 p0 = prevRingBase + static_cast<u32>(s);
				const u32 p1 = prevRingBase + static_cast<u32>(s) + 1;
				const u32 c0 = currRingBase + static_cast<u32>(s);
				const u32 c1 = currRingBase + static_cast<u32>(s) + 1;

				indices.push_back(p0);
				indices.push_back(c0);
				indices.push_back(p1);

				indices.push_back(p1);
				indices.push_back(c0);
				indices.push_back(c1);
			}
		}
	}

	generate_tip_cap(vertices, indices, progress, params, bevelRadius, bevelSegment, axisDir, uDir, vDir);

	if (vertices.empty() || indices.empty()) {
		return;
	}

	szg::ProceduralMeshBuilder builder;
	builder.add_submesh(std::move(vertices), std::move(indices), "clayMaterial")
		.set_material("clayMaterial", "clay.png");
	builder.build_and_register(meshName);
}

} // namespace

i32 ClayStretchMeshGenerator::DirectionIndex(const Vector3& direction) {

	return stretch_dir_index(direction);
}

ClayStretchParams ClayStretchMeshGenerator::LoadParams() {
	ClayStretchParams params;
	szg::JsonAsset param{ "[[game]]/ClayStretchAnimation.param" };
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
	params.squashRatio = std::clamp(readR32("SquashRatio", params.squashRatio), 0.0f, 1.0f);
	params.pullRatio = std::clamp(readR32("PullRatio", params.pullRatio), 0.0f, 1.0f);
	params.settleRatio = std::clamp(readR32("SettleRatio", params.settleRatio), 0.0f, 1.0f);
	params.pullProgress = std::clamp(readR32("PullProgress", params.pullProgress), 0.0f, 1.0f);
	params.neckDepth = std::clamp(readR32("NeckDepth", params.neckDepth), 0.0f, 1.0f);
	params.neckResolveStart = std::clamp(readR32("NeckResolveStart", params.neckResolveStart), 0.0f, 1.0f);
	params.tipDomeThreshold = std::clamp(readR32("TipDomeThreshold", params.tipDomeThreshold), 0.01f, 0.99f);
	params.ringCount = std::max(readI32("RingCount", params.ringCount), 2);
	params.cornerSegments = std::max(readI32("CornerSegments", params.cornerSegments), 1);
	params.keyframeCount = std::max(readI32("KeyframeCount", params.keyframeCount), 2);
	return params;
}

void ClayStretchMeshGenerator::GenerateAll() {
	const ClayStretchParams params = LoadParams();
	constexpr r32 kBevelRadius = 0.15f;
	constexpr i32 kBevelSegment = 3;

	for (i32 dirIdx = 0; dirIdx < NUM_DIRECTIONS; ++dirIdx) {
		const Vector3& direction = STRETCH_DIRS[dirIdx];

		for (i32 frame = 0; frame < params.keyframeCount; ++frame) {
			const r32 progress = (frame == 0) ? 0.0f : static_cast<r32>(frame) / static_cast<r32>(params.keyframeCount - 1);
			const std::string meshName = MeshName(dirIdx, frame);

			if (progress <= 0.001f) {
				szg::ProceduralMeshBuilder builder;
				std::vector<szg::VertexDataBuffer> emptyVerts;
				std::vector<u32> emptyIndices;
				builder.add_submesh(std::move(emptyVerts), std::move(emptyIndices), "clayMaterial")
					.set_material("clayMaterial", "clay.png");
				builder.build_and_register(meshName);
			} else {
				generate_single_frame(meshName, direction, progress, params, kBevelRadius, kBevelSegment);
			}
		}
	}
}
