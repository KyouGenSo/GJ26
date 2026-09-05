#include "PlayerDevScene.h"

#include <optional>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/PolygonMesh/PolygonMeshLibrary.h>
#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Module/World/Mesh/SkinningMeshInstance.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>
#include <Library/Utility/Tools/SmartPointer.h>

#include "Scripts/Instance/FollowCamera/FollowCamera.h"
#include "Scripts/Instance/Player/Player.h"
#include "Scripts/Manager/GoalManager.h"
#include "Scripts/ScriptMapTest/MapTestScript.h"

PlayerDevScene::PlayerDevScene() noexcept {
	set_name("PlayerDev");
}

PlayerDevScene::~PlayerDevScene() noexcept = default;

void PlayerDevScene::custom_load_asset() {
	szg::PolygonMeshLibrary::RegisterLoadQue("[[game]]/Cube.obj");
}

void PlayerDevScene::custom_setup() {
	Reference<szg::WorldCluster> world = world_mut(0);
	if (!world) {
		szgError("PlayerDev: world 0 not found.");
		return;
	}
	szg::WorldRoot& worldRoot = world->world_root_mut();

	std::unique_ptr<MapTestScript> mapTest = eps::CreateUnique<MapTestScript>();
	Reference<MapTestScript> mapTestRef = mapTest;
	mapTestRef->setup(worldRoot);

	// PlayerのWorldInstanceを取得
	auto playerInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::WorldInstance>>("RuntimeInstance", "Player");
	auto playerSkinningMeshInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::SkinningMeshInstance>>("RuntimeInstance", "PlayerMesh");
	auto playerStaticMeshInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::StaticMeshInstance>>("RuntimeInstance", "PlayerMesh");
	auto cameraInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::CameraInstance>>("RuntimeInstance", "MainCamera");
	auto cameraFollowTargetInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::WorldInstance>>("RuntimeInstance", "CameraFollowTarget");

	// Playerのスクリプトを作成
	playerScript = eps::CreateUnique<Player>(playerInstance.value_or(nullptr));
	Reference<Player> playerRef = playerScript;
	if (playerSkinningMeshInstance) {
		playerRef->set_mesh_instance(playerSkinningMeshInstance.value_or(nullptr));
	}
	else if (playerStaticMeshInstance) {
		playerRef->set_mesh_instance(playerStaticMeshInstance.value_or(nullptr));
	}
	else {
		szgWarning("PlayerDev: PlayerMesh runtime instance not found.");
	}
	playerRef->set_block_movement_judge(mapTestRef->movement_judge_mut());

	if (cameraInstance && cameraFollowTargetInstance) {
		followCameraScript = eps::CreateUnique<FollowCamera>(cameraInstance.value_or(nullptr), cameraFollowTargetInstance.value_or(nullptr));
		playerRef->set_follow_camera(followCameraScript);
		mapTestRef->set_follow_camera(followCameraScript);
	}
	else if (!cameraInstance) {
		szgWarning("PlayerDev: MainCamera runtime instance not found.");
	}
	else {
		szgWarning("PlayerDev: CameraFollowTarget runtime instance not found.");
	}

	mapTestRef->set_player(playerRef);

	std::unique_ptr<GoalManager> goalManager = eps::CreateUnique<GoalManager>();
	Reference<GoalManager> goalManagerRef = goalManager;
	goalManagerRef->setup(mapTestRef->field_mut(), worldRoot);
	goalManagerRef->set_player(playerRef);

	// ステージ更新 → Player移動 → 追従カメラ更新 → ゴール判定の順に実行する
	sceneScriptManager.register_script(std::move(mapTest));
	sceneScriptManager.register_script(std::move(playerScript));
	if (followCameraScript) {
		sceneScriptManager.register_script(std::move(followCameraScript));
	}
	sceneScriptManager.register_script(std::move(goalManager));
}
