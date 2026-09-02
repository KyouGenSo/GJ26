#include "PlayerDevScene.h"

#include "Scripts/Instance/Player/Player.h"
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


    playerScript = std::make_unique<Player>(playerInstance.value_or(nullptr));
    playerScript->set_mesh_instance(playerMeshInstance.value_or(nullptr));

    //ScriptManagerにPlayerを登録
    sceneScriptManager.register_script(std::move(playerScript));
}
