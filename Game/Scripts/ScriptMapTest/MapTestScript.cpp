#include "MapTestScript.h"

#include <algorithm>

#include <Engine/Application/Logger.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Runtime/Scene/SceneManager2.h>

#include "Scripts/Scene/FactoryGJ26.h"

namespace {

// 一時操作でマーカーの隣を探す順(±X, ±Z)
constexpr MapChipIndex kDirections[]{ { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

} // namespace

void MapTestScript::setup(Reference<szg::WorldRoot> worldRoot_) {
	worldRoot = worldRoot_;

	keys.initialize({ szg::KeyID::Left, szg::KeyID::Right, szg::KeyID::Up, szg::KeyID::Down, szg::KeyID::Z, szg::KeyID::F5 }, szg::InputInitializeMode::Current);
	pad.initialize({ szg::PadID::LShoulder, szg::PadID::RShoulder }, szg::InputInitializeMode::Current);

	stageCount = MapChipField::CountStages();
	if (stageCount == 0) {
		szgWarning("MapTestScript: '{}' not found.", MapChipField::StageDirectory(1));
		return;
	}
	stageNumber = std::clamp(szg::RuntimeStorage::GetValue<i32>("Temp", "StageNumber").value_or(1), 1, stageCount);

	// 地面
	ground = worldRoot->instantiate<szg::StaticMeshInstance>(nullptr, "Cube.obj");
	ground->get_materials()[0].color = ColorRGB{ 0.3f, 0.3f, 0.3f };

	// プレイヤー代わりのマーカー
	marker = worldRoot->instantiate<szg::StaticMeshInstance>(nullptr, "Cube.obj");
	marker->transform_mut().set_scale(Vector3{ 0.5f, 0.5f, 0.5f });
	marker->get_materials()[0].color = CColorRGB::BLUE;

	reload();
}

void MapTestScript::prev_update() {
	keys.update();
	pad.update();

	if (keys.trigger(szg::KeyID::F5)) {
		szg::SceneManager2::SceneChange(SceneListGJ26::StageEditor, 0.0f);
		return;
	}

	if (stageCount == 0) {
		return;
	}

	if (keys.trigger(szg::KeyID::Up) || keys.trigger(szg::KeyID::Down)) {
		debug_move_goal_piece(keys.trigger(szg::KeyID::Up));
		return;
	}
	if (keys.trigger(szg::KeyID::Z)) {
		debug_stretch_clay();
		return;
	}

	i32 step = 0;
	if (keys.trigger(szg::KeyID::Left) || pad.trigger(szg::PadID::LShoulder)) {
		step = -1;
	}
	if (keys.trigger(szg::KeyID::Right) || pad.trigger(szg::PadID::RShoulder)) {
		step = 1;
	}
	if (step == 0) {
		return;
	}

	stageNumber = (stageNumber - 1 + step + stageCount) % stageCount + 1;
	szg::RuntimeStorage::OverwirteValue("Temp", "StageNumber", i32{ stageNumber });
	reload();
}

void MapTestScript::reload() {
	field.load_stage(stageNumber);
	field.build(*worldRoot);

	const Vector3 center = MapChipField::to_world(field.width() - 1, 0, field.depth() - 1) * 0.5f;
	ground->transform_mut().set_scale(Vector3{ static_cast<r32>(field.width()), 0.1f, static_cast<r32>(field.depth()) });
	ground->transform_mut().set_translate(Vector3{ center.x, -0.55f, center.z });

	szgInformation("MapTestScript: stage {}/{} ({}x{}x{})", stageNumber, stageCount, field.width(), field.height(), field.depth());
}

/// <summary>
/// マーカーの前後左右にあるゴール条件オブジェクトを、押す(遠ざけてマーカーが 1 歩追う) / 引く(マーカーが 1 歩下がって寄せる)
/// </summary>
void MapTestScript::debug_move_goal_piece(bool push) {
	const std::optional<MapChipIndex> at = field.to_index(marker->transform_imm().get_translate());
	if (!at) {
		return;
	}
	for (const MapChipIndex& d : kDirections) {
		const MapChipIndex piece{ at->x + d.x, at->y, at->z + d.z };
		if (field.get(piece.x, piece.y, piece.z) != MapChipType::GoalPiece) {
			continue;
		}
		const MapChipIndex pieceTo = push ? MapChipIndex{ piece.x + d.x, piece.y, piece.z + d.z } : *at;
		const MapChipIndex markerTo = push ? piece : MapChipIndex{ at->x - d.x, at->y, at->z - d.z };
		if (!push && field.get(markerTo.x, markerTo.y, markerTo.z) != MapChipType::Empty) {
			szgInformation("MapTestScript: pull ng (marker blocked)");
			return;
		}
		const bool moved = field.move_goal_piece(piece, pieceTo);
		if (moved) {
			marker->transform_mut().set_translate(MapChipField::to_world(markerTo.x, markerTo.y, markerTo.z));
		}
		szgInformation("MapTestScript: {} piece ({},{},{}) -> ({},{},{}) {}",
			push ? "push" : "pull", piece.x, piece.y, piece.z, pieceTo.x, pieceTo.y, pieceTo.z, moved ? "ok" : "ng");
		return;
	}
}

/// <summary>
/// マーカーの前後左右にある粘土を、±X ±Z の順で最初に伸ばせた方向へ 1 マス伸ばす(伸ばす先がゴール条件オブジェクトならつながる)
/// </summary>
void MapTestScript::debug_stretch_clay() {
	const std::optional<MapChipIndex> at = field.to_index(marker->transform_imm().get_translate());
	if (!at) {
		return;
	}
	for (const MapChipIndex& d : kDirections) {
		const MapChipIndex clay{ at->x + d.x, at->y, at->z + d.z };
		if (field.get(clay.x, clay.y, clay.z) != MapChipType::Clay) {
			continue;
		}
		for (const MapChipIndex& e : kDirections) {
			const MapChipIndex to{ clay.x + e.x, clay.y, clay.z + e.z };
			if (field.stretch_clay(clay, to)) {
				szgInformation("MapTestScript: stretch ({},{},{}) -> ({},{},{}) ok", clay.x, clay.y, clay.z, to.x, to.y, to.z);
				return;
			}
		}
		szgInformation("MapTestScript: stretch ({},{},{}) ng", clay.x, clay.y, clay.z);
		return;
	}
}
