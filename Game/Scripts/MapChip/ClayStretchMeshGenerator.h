#pragma once

#include <format>
#include <string>
#include <vector>

#include <Library/Math/Vector3.h>
#include <Library/Utility/BuiltinTypes.h>

struct ClayStretchParams {
	r32 squashRatio = 0.15f;
	r32 pullRatio = 0.55f;
	r32 settleRatio = 0.30f;
	r32 pullProgress = 0.85f;
	r32 neckDepth = 0.30f;
	r32 neckResolveStart = 0.70f;
	r32 tipDomeThreshold = 0.40f;
	i32 ringCount = 8;
	i32 cornerSegments = 4;
	i32 keyframeCount = 10;
};

class ClayStretchMeshGenerator {
public:
	static constexpr i32 NUM_DIRECTIONS = 4;

	static std::string MeshName(i32 dirIndex, i32 frameIndex) {
		return std::format("__clay_stretch_{}_{}", dirIndex, frameIndex);
	}

	static i32 DirectionIndex(const Vector3& direction);

	static ClayStretchParams LoadParams();

	static void GenerateAll();

private:
	static void GenerateSingleFrame(
		const std::string& meshName,
		const Vector3& direction,
		r32 progress,
		const ClayStretchParams& params,
		r32 bevelRadius,
		i32 bevelSegment);
};
