#include "PlayerDevScene.h"

#include "Scripts/Instance/FollowCamera/FollowCamera.h"
#include "Scripts/Instance/Player/Player.h"
#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>

PlayerDevScene::PlayerDevScene() noexcept {
	set_name("PlayerDev");
}

PlayerDevScene::~PlayerDevScene() noexcept = default;

void PlayerDevScene::custom_setup() {

	// PlayerのWorldInstanceを取得
    auto playerInstance = 
        szg::RuntimeStorage::GetValue<Reference<szg::WorldInstance>>("RuntimeInstance", "Player");
	auto playerMeshInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::SkinningMeshInstance>>("RuntimeInstance", "PlayerMesh");
	auto cameraInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::CameraInstance>>("RuntimeInstance", "MainCamera");

    playerScript = std::make_unique<Player>(playerInstance.value_or(nullptr));
    playerScript->set_mesh_instance(playerMeshInstance.value_or(nullptr));
	followCameraScript = std::make_unique<FollowCamera>(
		cameraInstance.value_or(nullptr),
		Reference<Player>{ playerScript }
	);
	playerScript->set_follow_camera(followCameraScript);

	// Playerの移動後にFollowCameraを更新するため、この順番で登録する
    sceneScriptManager.register_script(std::move(playerScript));
	sceneScriptManager.register_script(std::move(followCameraScript));
}
