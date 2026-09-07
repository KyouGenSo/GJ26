#include "GamePlayScript.h"

#include <Engine/Application/Logger.h>
#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Module/World/Mesh/SkinningMeshInstance.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Runtime/Scene/World/WorldRoot.h>
#include <Library/Utility/Tools/SmartPointer.h>

#include "Scripts/Instance/FollowCamera/FollowCamera.h"
#include "Scripts/Instance/Player/Player.h"
#include "Scripts/Manager/GoalManager.h"
#include "Scripts/ScriptMapTest/MapTestScript.h"

void GamePlayScript::setup(Reference<szg::WorldRoot> worldRoot) {
	if (isSetup_) {
		szgWarning("GamePlayScript: setup was called more than once.");
		return;
	}
	if (!worldRoot) {
		szgError("GamePlayScript: WorldRoot not found.");
		return;
	}

	std::unique_ptr<MapTestScript> mapTest = eps::CreateUnique<MapTestScript>();
	mapTest_ = mapTest;
	mapTest_->setup(worldRoot);

	const auto playerInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::WorldInstance>>("RuntimeInstance", "Player");
	const auto playerMeshInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::SkinningMeshInstance>>("RuntimeInstance", "PlayerMesh");
	const auto cameraInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::CameraInstance>>("RuntimeInstance", "MainCamera");
	const auto cameraFollowTargetInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::WorldInstance>>("RuntimeInstance", "CameraFollowTarget");

	// プレイヤーのスクリプト
	std::unique_ptr<Player> player = 
		eps::CreateUnique<Player>(playerInstance.value_or(nullptr),playerMeshInstance.value_or(nullptr));

	player_ = player;
	if (playerInstance) {
		player_->set_world_instance(playerInstance.value_or(nullptr));
	}
	else {
		szgWarning("GamePlayScript: Player runtime instance not found.");
	}
	player_->set_block_movement_judge(mapTest_->movement_judge_mut());

	std::unique_ptr<FollowCamera> followCamera;
	if (cameraInstance && cameraFollowTargetInstance) {
		followCamera = eps::CreateUnique<FollowCamera>(
			cameraInstance.value_or(nullptr),
			cameraFollowTargetInstance.value_or(nullptr));
		followCamera_ = followCamera;
		player_->set_follow_camera(followCamera_);
		mapTest_->set_follow_camera(followCamera_);
	}
	else if (!cameraInstance) {
		szgWarning("GamePlayScript: MainCamera runtime instance not found.");
	}
	else {
		szgWarning("GamePlayScript: CameraFollowTarget runtime instance not found.");
	}

	mapTest_->set_player(player_);

	std::unique_ptr<GoalManager> goalManager = eps::CreateUnique<GoalManager>();
	goalManager_ = goalManager;
	goalManager_->setup(mapTest_->field_mut(), worldRoot);
	goalManager_->set_player(player_);

	// ステージ更新 -> Player移動 -> 追従カメラ更新 -> ゴール判定の順に実行する
	inGameScriptManager_.register_script(std::move(mapTest));
	inGameScriptManager_.register_script(std::move(player));
	if (followCamera) {
		inGameScriptManager_.register_script(std::move(followCamera));
	}
	inGameScriptManager_.register_script(std::move(goalManager));

	isSetup_ = true;
}

void GamePlayScript::finalize() {
	if (!isSetup_) {
		return;
	}

	inGameScriptManager_.finalize();
	mapTest_.reset();
	player_.reset();
	followCamera_.reset();
	goalManager_.reset();
}

void GamePlayScript::prev_update() {
	if (!isSetup_) {
		return;
	}

	inGameScriptManager_.prev_update();

	// ポーズなど、各要素の更新後に行うインゲーム全体処理をここへ追加する。
}

void GamePlayScript::post_update() {
	if (!isSetup_) {
		return;
	}

	inGameScriptManager_.post_update();

	// ステージクリア後の遷移など、判定後の処理をここへ追加する。
}
