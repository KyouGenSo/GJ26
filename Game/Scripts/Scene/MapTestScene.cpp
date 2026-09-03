#include "MapTestScene.h"

#include <memory>

#include <optional>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/PolygonMesh/PolygonMeshLibrary.h>
#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Module/World/WorldInstance/WorldInstance.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>
#include <Library/Utility/Tools/SmartPointer.h>

#include "Scripts/Instance/Player/Player.h"
#include "Scripts/Manager/GoalManager.h"
#include "Scripts/Instance/FollowCamera/FollowCamera.h"
#include "Scripts/Instance/Player/Player.h"
#include "Scripts/ScriptMapTest/MapTestScript.h"

MapTestScene::MapTestScene() {
	set_name("MapTest");
}

void MapTestScene::custom_load_asset() {
	szg::PolygonMeshLibrary::RegisterLoadQue("[[game]]/Cube.obj");
	szg::PolygonMeshLibrary::RegisterLoadQue("[[szg]]/Primitive/Sphere.obj");
}

void MapTestScene::custom_setup() {
	Reference<szg::WorldCluster> world = world_mut(0);
	if (!world) {
		szgError("MapTest: world 0 not found.");
		return;
	}
	szg::WorldRoot& worldRoot = world->world_root_mut();

	std::unique_ptr<MapTestScript> mapTest = eps::CreateUnique<MapTestScript>();
	Reference<MapTestScript> mapTestRef = mapTest;
	mapTestRef->setup(worldRoot);

	// マップ操作確認用の仮プレイヤー
	Reference<szg::WorldInstance> playerInstance = worldRoot.instantiate<szg::WorldInstance>(nullptr);
	Reference<szg::StaticMeshInstance> playerMesh =
		worldRoot.instantiate<szg::StaticMeshInstance>(playerInstance, "Sphere.obj");
	playerMesh->transform_mut().set_scale(Vector3{ 0.35f, 0.35f, 0.35f });
	playerMesh->transform_mut().set_translate(Vector3{ 0.0f, 0.35f, 0.0f });
	if (!playerMesh->get_materials().empty()) {
		playerMesh->get_materials()[0].color = ColorRGB{ 0.2f, 0.6f, 1.0f };
	}

	std::unique_ptr<Player> player = eps::CreateUnique<Player>(playerInstance);
	Reference<Player> playerRef = player;
	playerRef->set_block_movement_judge(mapTestRef->movement_judge_imm());

	std::unique_ptr<FollowCamera> followCamera;
	const std::optional<Reference<szg::CameraInstance>> camera =
		szg::RuntimeStorage::GetValue<Reference<szg::CameraInstance>>("RuntimeInstance", "MainCamera");
	if (camera) {
		followCamera = eps::CreateUnique<FollowCamera>(*camera, playerRef);
		playerRef->set_follow_camera(followCamera);
	}
	else {
		szgWarning("MapTest: MainCamera runtime instance not found.");
	}

	mapTestRef->set_player(playerRef);

	// ステージ更新 → Player移動 → 追従カメラ更新の順に実行する
	sceneScriptManager.register_script(std::move(mapTest));
	sceneScriptManager.register_script(std::move(player));
	if (followCamera) {
		sceneScriptManager.register_script(std::move(followCamera));
	}
	mapTestRef->setup(world->world_root_mut());

	// 登録順 = 更新順。Player がマーカーを動かした後に GoalManager が判定する);
	std::unique_ptr<GoalManager> goalManager = eps::CreateUnique<GoalManager>();
	Reference<GoalManager> goalRef = goalManager;
	goalRef->setup(mapTestRef->field_mut(), world->world_root_mut());
	goalRef->set_player(playerRef);
	sceneScriptManager.register_script(std::move(goalManager));
}
