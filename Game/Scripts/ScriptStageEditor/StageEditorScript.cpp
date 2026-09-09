#include "StageEditorScript.h"

#include <Engine/Application/Logger.h>
#include <Engine/Runtime/Scene/SceneManager2.h>

#include "Scripts/Editor/StageEditorDocument.h"
#include "Scripts/MapChip/MapChipField.h"
#include "Scripts/Scene/FactoryGJ26.h"
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>

ColorRGB StageEditorScript::ChipColor(MapChipType type, u8 clayColor) {
	switch (type) {
	case MapChipType::Clay:
		return ClayColor::Preview[clayColor];
	case MapChipType::GoalPiece:
		return CColorRGB::YELLOW;
	case MapChipType::Goal:
		return CColorRGB::GREEN;
	default:
		return CColorRGB::WHITE;
	}
}

void StageEditorScript::setup(Reference<szg::WorldRoot> worldRoot_) {
	worldRoot = worldRoot_;

	keys.initialize({ szg::KeyID::F5 }, szg::InputInitializeMode::Current);

	// 地面
	ground = worldRoot->instantiate<szg::StaticMeshInstance>(nullptr, "Cube.obj");
	if (!ground->get_materials().empty()) {
		ground->get_materials()[0].color = ColorRGB{ 0.3f, 0.3f, 0.3f };
	}

	rebuild();
}

void StageEditorScript::prev_update() {
	keys.update();

	StageEditorDocument& doc = StageEditorDocument::GetInstance();
	if (doc.version() != lastVersion) {
		rebuild();
	}

	if (keys.trigger(szg::KeyID::F5)) {
		szg::RuntimeStorage::OverwirteValue("Temp", "StageNumber", i32{ doc.stage_number() });
		szg::SceneManager2::SceneChange(SceneListGJ26::GamePlay, 0.0f);
	}
}

void StageEditorScript::rebuild() {
	StageEditorDocument& doc = StageEditorDocument::GetInstance();
	lastVersion = doc.version();

	// 既存キューブを破棄
	for (auto& cube : cubes) {
		if (cube) {
			cube->destroy_self();
		}
	}
	cubes.clear();

	if (doc.width() <= 0 || doc.height() <= 0 || doc.depth() <= 0) {
		return;
	}

	// 地面を配置
	const Vector3 center = MapChipField::to_world(doc.width() - 1, 0, doc.depth() - 1) * 0.5f;
	ground->transform_mut().set_scale(Vector3{ static_cast<r32>(doc.width()), 0.1f, static_cast<r32>(doc.depth()) });
	ground->transform_mut().set_translate(Vector3{ center.x, -0.55f, center.z });

	// チップを再配置
	const bool singleLayer = doc.preview_single_layer();
	const i32 previewY = doc.current_layer() - 1;
	cubes.reserve(static_cast<size_t>(doc.width() * doc.height() * doc.depth()));
	for (i32 y = 0; y < doc.height(); ++y) {
		if (singleLayer && y != previewY) {
			continue;
		}
		for (i32 z = 0; z < doc.depth(); ++z) {
			for (i32 x = 0; x < doc.width(); ++x) {
				MapChipType chip = doc.get(x, y, z);
				if (chip == MapChipType::Empty) {
					cubes.emplace_back();
					continue;
				}

				Reference<szg::StaticMeshInstance> cube = worldRoot->instantiate<szg::StaticMeshInstance>(nullptr, "Cube.obj");
				cube->transform_mut().set_translate(MapChipField::to_world(x, y, z));
				if (!cube->get_materials().empty()) {
					cube->get_materials()[0].color = ChipColor(chip, doc.clay_color(x, y, z));
				}
				if (chip == MapChipType::Clay) {
					// Cube.obj は中心原点の 1 辺 1 なので半幅 0.5・底面 -0.5
					MapChipField::AttachFaceCrosses(*worldRoot, cube, doc.blocked_faces(x, y, z), 0.5f, -0.5f);
				}
				cubes.emplace_back(cube);
			}
		}
	}

	// プレイヤー初期位置は青い半分サイズの立方体、向きは小さな立方体で示す
	if (const std::optional<PlayerSpawnRecord>& spawn = doc.player_spawn()) {
		const Vector3 position = MapChipField::to_world(spawn->position.x, spawn->position.y, spawn->position.z);
		const MapChipIndex d = ClayFace::ToDirection(spawn->direction);
		const Vector3 direction = MapChipField::to_world(d.x, d.y, d.z);
		const auto place = [this](const Vector3& at, r32 scale) {
			Reference<szg::StaticMeshInstance> cube = worldRoot->instantiate<szg::StaticMeshInstance>(nullptr, "Cube.obj");
			cube->transform_mut().set_translate(at);
			cube->transform_mut().set_scale(Vector3{ scale, scale, scale });
			if (!cube->get_materials().empty()) {
				cube->get_materials()[0].color = CColorRGB::BLUE;
			}
			cubes.emplace_back(cube);
		};
		place(position, 0.5f);
		place(position + direction * 0.4f, 0.2f);
	}
}
