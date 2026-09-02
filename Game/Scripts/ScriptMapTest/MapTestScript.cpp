#include "MapTestScript.h"

#include <algorithm>

#include <Engine/Application/Logger.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>

void MapTestScript::setup(Reference<szg::WorldRoot> worldRoot_) {
	worldRoot = worldRoot_;
	stageCount = MapChipField::CountStages();
	if (stageCount == 0) {
		szgWarning("MapTestScript: \'{}\' not found.", MapChipField::StageDirectory(1));
		return;
	}
	stageNumber = std::clamp(szg::RuntimeStorage::GetValue<i32>("Temp", "StageNumber").value_or(1), 1, stageCount);

	keys.initialize({ szg::KeyID::Left, szg::KeyID::Right });
	pad.initialize({ szg::PadID::LShoulder, szg::PadID::RShoulder });

	// 地面
	ground = worldRoot->instantiate<szg::StaticMeshInstance>(nullptr, "Cube.obj");
	ground->get_materials()[0].color = ColorRGB{ 0.3f, 0.3f, 0.3f };

	reload();
}

void MapTestScript::prev_update() {
	if (stageCount == 0) {
		return;
	}
	keys.update();
	pad.update();

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
