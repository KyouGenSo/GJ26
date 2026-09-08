#pragma once

#include <format>
#include <string>

#include <Library/Utility/BuiltinTypes.h>

class MapChipField;

class ClayMeshGenerator {
public:
	static std::string MeshName(i32 originFlat) {
		return std::format("__clay_block_{}", originFlat);
	}

	static void Generate(i32 originFlat, const MapChipField& field);

	static void Remove(i32 originFlat) {
		Remove(MeshName(originFlat));
	}

	static void Remove(const std::string& meshName);
};
