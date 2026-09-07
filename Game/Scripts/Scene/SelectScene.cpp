#include "SelectScene.h"

#include <Engine/Application/Logger.h>
#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Module/World/Light/DirectionalLight/DirectionalLightInstance.h>
#include <Engine/Module/World/Mesh/Primitive/Rect3d.h>
#include <Engine/Module/World/Mesh/Primitive/StringRectInstance.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>
#include <Library/Utility/Tools/SmartPointer.h>

#include "Scripts/MapChip/MapChipField.h"
#include "Scripts/ScriptStageSelect/StageSelectScript.h"

SelectScene::SelectScene() noexcept {
	set_name("SelectScene");
}

void SelectScene::custom_load_asset() {
	MapChipField::RegisterVisualAssets();
}

void SelectScene::custom_setup() {
	Reference<szg::WorldCluster> world = world_mut(0);
	if (!world) {
		szgError("SelectScene: world 0 not found.");
		return;
	}

	szg::WorldRoot& worldRoot = world->world_root_mut();
	Reference<szg::DirectionalLightInstance> light =
		worldRoot.instantiate<szg::DirectionalLightInstance>(nullptr);
	light->light_data_mut().direction = Vector3{ 0.32f, -0.80f, 0.48f };
	light->light_data_mut().intensity = 1.0f;
	light->set_influence_layer(1);

	const auto stageNumberText =
		szg::RuntimeStorage::GetValue<Reference<szg::StringRectInstance>>("RuntimeInstance", "StageNumber");
	const auto previewCamera =
		szg::RuntimeStorage::GetValue<Reference<szg::CameraInstance>>("RuntimeInstance", "3dCamera");
	const auto leftArrow =
		szg::RuntimeStorage::GetValue<Reference<szg::Rect3d>>("RuntimeInstance", "SelectAllowSprite_Left");
	const auto rightArrow =
		szg::RuntimeStorage::GetValue<Reference<szg::Rect3d>>("RuntimeInstance", "SelectAllowSprite_Right");

	std::unique_ptr<StageSelectScript> script = eps::CreateUnique<StageSelectScript>();
	script->setup(
		worldRoot,
		previewCamera.value_or(nullptr),
		stageNumberText.value_or(nullptr),
		leftArrow.value_or(nullptr),
		rightArrow.value_or(nullptr));
	sceneScriptManager.register_script(std::move(script));
}
