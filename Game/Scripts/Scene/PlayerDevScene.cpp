#include "PlayerDevScene.h"

#include "Scripts/Instance/Player/Player.h"
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>

PlayerDevScene::PlayerDevScene() noexcept {
	set_name("PlayerDev");
}

void PlayerDevScene::custom_setup() {

	// PlayerのWorldInstanceを取得
    auto playerInstance = 
        szg::RuntimeStorage::GetValue<Reference<szg::WorldInstance>>("RuntimeInstance", "Player");

	//ScriptManagerにPlayerを登録
    sceneScriptManager.register_script(std::make_unique<Player>(*playerInstance));
}
