#include "MapTestScene.h"

#include <Engine/Application/Logger.h>
#include <Engine/Assets/PolygonMesh/PolygonMeshLibrary.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>
#include <Library/Utility/Tools/SmartPointer.h>

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
}
