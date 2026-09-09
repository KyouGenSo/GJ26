#include "SelectScene.h"

#include <Engine/Application/Logger.h>
#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Module/World/Light/DirectionalLight/DirectionalLightInstance.h>
#include <Engine/Module/World/Mesh/Primitive/Rect3d.h>
#include <Engine/Module/World/Mesh/Primitive/StringRectInstance.h>
#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>
#include <Library/Utility/Tools/SmartPointer.h>

#include "Scripts/Instance/Skydome/Skydome.h"
#include "Scripts/MapChip/MapChipField.h"
#include "Scripts/ScriptStageSelect/StageSelectScript.h"
#include "Scripts/ScriptStageSelect/StageSelectBackgroundEffect.h"

SelectScene::SelectScene() noexcept {
	set_name("SelectScene");
}

void SelectScene::custom_load_asset() {
	MapChipField::RegisterVisualAssets();
	StageSelectBackgroundEffect::RegisterVisualAssets();
	StageSelectScript::RegisterAudioAssets();
}

void SelectScene::custom_setup() {
	Reference<szg::WorldCluster> world = world_mut(0);
	if (!world) {
		szgError("SelectScene: world 0 not found.");
		return;
	}

	szg::WorldRoot& worldRoot = world->world_root_mut();
	worldRoot.instantiate<Skydome>(nullptr);
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

	auto background = eps::CreateUnique<StageSelectBackgroundEffect>();
	background->setup(worldRoot, previewCamera.value_or(nullptr));
	sceneScriptManager.register_script(std::move(background));

	// クリア済みステージの印。UI ワールド(正射影カメラ)に置く
	Reference<szg::StaticMeshInstance> clearBadge;
	if (Reference<szg::WorldCluster> uiWorld = world_mut(1)) {
		clearBadge = uiWorld->world_root_mut().instantiate<szg::StaticMeshInstance>(nullptr, "goal.obj");
	}
	else {
		szgWarning("SelectScene: world 1 (UI) not found. Clear badge is disabled.");
	}

	std::unique_ptr<StageSelectScript> script = eps::CreateUnique<StageSelectScript>();
	script->setup(
		worldRoot,
		previewCamera.value_or(nullptr),
		stageNumberText.value_or(nullptr),
		leftArrow.value_or(nullptr),
		rightArrow.value_or(nullptr),
		clearBadge);
	sceneScriptManager.register_script(std::move(script));
}
