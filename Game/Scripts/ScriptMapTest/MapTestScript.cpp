#include "MapTestScript.h"

#include <algorithm>

#include <Engine/Application/Logger.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Runtime/Scene/SceneManager2.h>

#include "Scripts/Scene/FactoryGJ26.h"

void MapTestScript::setup(Reference<szg::WorldRoot> worldRoot_) {
	worldRoot = worldRoot_;

	keys.initialize({ szg::KeyID::Left, szg::KeyID::Right, szg::KeyID::Up, szg::KeyID::Down, szg::KeyID::F5 }, szg::InputInitializeMode::Current);
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
	// ponytail: Player の Push / Pull が move_goal_piece を呼ぶようになったら削除する
	const std::optional<MapChipIndex> at = field.to_index(marker->transform_imm().get_translate());
	if (!at) {
		return;
	}
	constexpr MapChipIndex kDirections[]{ { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	for (const MapChipIndex& d : kDirections) {
		const MapChipIndex piece{ at->x + d.x, at->y, at->z + d.z };
		if (field.get(piece.x, piece.y, piece.z) != MapChipType::GoalPiece) {
			continue;
		}
		const MapChipIndex pieceTo = push ? MapChipIndex{ piece.x + d.x, piece.y, piece.z + d.z } : *at;
		const MapChipIndex markerTo = push ? piece : MapChipIndex{ at->x - d.x, at->y, at->z - d.z };
		if (!push && field.get(markerTo.x, markerTo.y, markerTo.z) != MapChipType::Empty) {
			return;
		}
		if (field.move_goal_piece(piece, pieceTo)) {
			marker->transform_mut().set_translate(MapChipField::to_world(markerTo.x, markerTo.y, markerTo.z));
		}
		return;
	}
}
