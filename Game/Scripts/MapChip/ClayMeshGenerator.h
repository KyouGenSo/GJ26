#pragma once

#include <format>
#include <string>

#include <Library/Utility/BuiltinTypes.h>

class MapChipField;

class ClayMeshGenerator {
public:
	/// <summary>
	/// clay block のメッシュ名（OBJ ファイル名と一致する）
	/// <para>形式: "__clay_block_{stage:02}_{origin}.obj"</para>
	/// <para>PolygonMeshLibrary のキーとして使われるため .obj 拡張子を含む</para>
	/// </summary>
	static std::string MeshName(i32 stageNumber, i32 originFlat) {
		return std::format("__clay_block_{:02}_{}.obj", stageNumber, originFlat);
	}

	static void Generate(i32 stageNumber, i32 originFlat, const MapChipField& field, u8 clayColor = 0);

	static void Remove(i32 stageNumber, i32 originFlat) {
		Remove(MeshName(stageNumber, originFlat));
	}

	static void Remove(const std::string& meshName);
};
