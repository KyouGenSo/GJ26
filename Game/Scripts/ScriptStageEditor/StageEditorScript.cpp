#include "StageEditorScript.h"

#include <Engine/Application/Logger.h>
#include <Engine/Runtime/Scene/SceneManager2.h>

#include "Scripts/Editor/StageEditorDocument.h"
#include "Scripts/MapChip/MapChipField.h"
#include "Scripts/Scene/FactoryGJ26.h"

ColorRGB StageEditorScript::ChipColor(MapChipType type) {
	switch (type) {
	case MapChipType::Clay:
		return ColorRGB{ 0.55f, 0.35f, 0.20f };
	case MapChipType::GoalPiece:
		return CColorRGB::YELLOW;
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
		szg::SceneManager2::SceneChange(SceneListGJ26::MapTest, 0.0f);
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
					cube->get_materials()[0].color = ChipColor(chip);
				}
				cubes.emplace_back(cube);
			}
		}
	}
}
