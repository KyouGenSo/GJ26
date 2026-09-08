#include "GamePlayScene.h"

#include <Engine/Application/Logger.h>
#include <Engine/Assets/Shader/ShaderLibrary.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>
#include <Library/Utility/Tools/SmartPointer.h>

#include "Scripts/MapChip/MapChipField.h"
#include "Scripts/ScriptGamePlay/GamePlayScript.h"

GamePlayScene::GamePlayScene() noexcept {
	set_name("GamePlay");
}

GamePlayScene::~GamePlayScene() noexcept = default;

void GamePlayScene::custom_load_asset() {
	MapChipField::RegisterVisualAssets();
	// RenderPath.json のブルーム(レイヤー 1 をぼかして加算)が使うポストエフェクトシェーダー
	szg::ShaderLibrary::RegisterLoadQue("[[szg]]/PostEffect/GaussianBlur.PS.hlsl");
	szg::ShaderLibrary::RegisterLoadQue("[[szg]]/PostEffect/MargeTexture4.PS.hlsl");
	szg::ShaderLibrary::RegisterLoadQue("[[szg]]/PostEffect/Bloom.PS.hlsl");
	szg::TextureLibrary::RegisterLoadQue("./Game/Assets/Models/floor/floor.png");
}

void GamePlayScene::custom_setup() {
	Reference<szg::WorldCluster> world = world_mut(0);
	if (!world) {
		szgError("GamePlay: world 0 not found.");
		return;
	}

	std::unique_ptr<GamePlayScript> gamePlayScript = eps::CreateUnique<GamePlayScript>();
	gamePlayScript->setup(world->world_root_mut());
	sceneScriptManager.register_script(std::move(gamePlayScript));
}
