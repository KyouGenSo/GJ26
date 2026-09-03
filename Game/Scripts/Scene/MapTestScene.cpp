#include "MapTestScene.h"

#include <memory>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/PolygonMesh/PolygonMeshLibrary.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>
#include <Library/Utility/Tools/SmartPointer.h>

#include "Scripts/Instance/Player/Player.h"
#include "Scripts/Manager/GoalManager.h"
#include "Scripts/ScriptMapTest/MapTestScript.h"

MapTestScene::MapTestScene() {
	set_name("MapTest");
}

void MapTestScene::custom_load_asset() {
	szg::PolygonMeshLibrary::RegisterLoadQue("[[game]]/Cube.obj");
}

void MapTestScene::custom_setup() {
	std::unique_ptr<MapTestScript> mapTest = eps::CreateUnique<MapTestScript>();
	Reference<MapTestScript> mapTestRef = mapTest;
	sceneScriptManager.register_script(std::move(mapTest));

	Reference<szg::WorldCluster> world = world_mut(0);
	if (!world) {
		szgError("MapTest: world 0 not found.");
		return;
	}
	mapTestRef->setup(world->world_root_mut());

	// 登録順 = 更新順。Player がマーカーを動かした後に GoalManager が判定する
	std::unique_ptr<Player> player = std::make_unique<Player>(mapTestRef->marker_mut());
	Reference<Player> playerRef = player;
	sceneScriptManager.register_script(std::move(player));

	std::unique_ptr<GoalManager> goalManager = eps::CreateUnique<GoalManager>();
	Reference<GoalManager> goalRef = goalManager;
	goalRef->setup(mapTestRef->field_mut(), world->world_root_mut());
	goalRef->set_player(playerRef);
	sceneScriptManager.register_script(std::move(goalManager));
}
