#include "MapTestScript.h"

#include <algorithm>
#include <array>
#include <optional>

#include <Engine/Application/Logger.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Runtime/Scene/SceneManager2.h>

#include "Scripts/Instance/FollowCamera/FollowCamera.h"
#include "Scripts/Instance/Player/Player.h"
#include "Scripts/Scene/FactoryGJ26.h"

namespace {

struct TemporaryPlayerSpawn {
	Vector3 position;
	Vector3 direction;
};

std::optional<TemporaryPlayerSpawn> FindTemporaryPlayerSpawn(const MapChipField& field) {
	constexpr std::array<MapChipIndex, 4> directions{
		MapChipIndex{ 0, 0, 1 },
		MapChipIndex{ 1, 0, 0 },
		MapChipIndex{ 0, 0, -1 },
		MapChipIndex{ -1, 0, 0 },
	};

	std::optional<MapChipIndex> firstEmpty;
	for (i32 z = 0; z < field.depth(); ++z) {
		for (i32 x = 0; x < field.width(); ++x) {
			const MapChipIndex playerIndex{ x, 0, z };
			if (field.get(x, 0, z) != MapChipType::Empty) {
				continue;
			}
			if (!firstEmpty) {
				firstEmpty = playerIndex;
			}

			for (const MapChipIndex& direction : directions) {
				const MapChipIndex target{
					x + direction.x,
					0,
					z + direction.z,
				};
				if (field.contains(target) &&
					field.get(target.x, target.y, target.z) == MapChipType::GoalPiece) {
					return TemporaryPlayerSpawn{
						MapChipField::to_world(x, 0, z),
						Vector3{
							static_cast<r32>(direction.x),
							0.0f,
							static_cast<r32>(direction.z),
						},
					};
				}
			}
		}
	}

	if (!firstEmpty) {
		return std::nullopt;
	}
	return TemporaryPlayerSpawn{
		MapChipField::to_world(firstEmpty->x, firstEmpty->y, firstEmpty->z),
		Vector3{ 0.0f, 0.0f, 1.0f },
	};
}

// 一時操作でマーカーの隣を探す順(±X, ±Z)
constexpr MapChipIndex kDirections[]{ { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };


} // namespace

void MapTestScript::setup(Reference<szg::WorldRoot> worldRoot_) {
	worldRoot = worldRoot_;
	blockMovementJudge.set_field(field);

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

void MapTestScript::set_player(Reference<Player> player_) {
	player = player_;
	if (marker) {
		marker->set_active(!player);
	}
	reset_player_position();
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

	reset_player_position();
	szgInformation("MapTestScript: stage {}/{} ({}x{}x{})", stageNumber, stageCount, field.width(), field.height(), field.depth());
}

void MapTestScript::reset_player_position() {
	if (!player || !player->get_world_instance_mut()) {
		return;
	}

	const std::optional<TemporaryPlayerSpawn> spawn = FindTemporaryPlayerSpawn(field);
	if (!spawn) {
		szgWarning("MapTestScript: temporary player spawn was not found.");
		return;
	}

	player->get_world_instance_mut()->transform_mut().set_translate(spawn->position);
	player->set_direction(spawn->direction);
	if (Reference<FollowCamera> followCamera = player->get_follow_camera_mut()) {
		followCamera->request_snap();
	}
}
/// <summary>
/// Player（未設定ならマーカー）の前後左右にあるGoalPieceを押す / 引く
/// </summary>
void MapTestScript::debug_move_goal_piece(bool push) {
	// ponytail: Player の Push / Pull が move_goal_piece を呼ぶようになったら削除する
	Reference<szg::WorldInstance> actor =
		player ? player->get_world_instance_mut() : Reference<szg::WorldInstance>{ marker };
	if (!actor) {
		return;
	}

	const std::optional<MapChipIndex> at = field.to_index(actor->transform_imm().get_translate());
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
			actor->transform_mut().set_translate(MapChipField::to_world(markerTo.x, markerTo.y, markerTo.z));
			if (player) {
				player->set_direction(Vector3{
					static_cast<r32>(d.x),
					0.0f,
					static_cast<r32>(d.z),
				});
			}
		}
		szgInformation("MapTestScript: {} piece ({},{},{}) -> ({},{},{}) {}",
			push ? "push" : "pull", piece.x, piece.y, piece.z, pieceTo.x, pieceTo.y, pieceTo.z, moved ? "ok" : "ng");
		return;
	}
}
