#include "StageEditorScene.h"

#include <Engine/Application/Logger.h>
#include <Engine/Assets/PolygonMesh/PolygonMeshLibrary.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>
#include <Library/Utility/Tools/SmartPointer.h>

#include "Scripts/ScriptStageEditor/StageEditorCameraScript.h"
#include "Scripts/ScriptStageEditor/StageEditorScript.h"

StageEditorScene::StageEditorScene() {
	set_name("StageEditor");
}

void StageEditorScene::custom_load_asset() {
	szg::PolygonMeshLibrary::RegisterLoadQue("[[game]]/Cube.obj");
}

void StageEditorScene::custom_setup() {
	std::unique_ptr<StageEditorScript> script = eps::CreateUnique<StageEditorScript>();
	Reference<StageEditorScript> scriptRef = script;
	sceneScriptManager.register_script(std::move(script));

	std::unique_ptr<StageEditorCameraScript> cameraScript = eps::CreateUnique<StageEditorCameraScript>();
	Reference<StageEditorCameraScript> cameraScriptRef = cameraScript;
	sceneScriptManager.register_script(std::move(cameraScript));

	Reference<szg::WorldCluster> world = world_mut(0);
	if (!world) {
		szgError("StageEditorScene: world 0 not found.");
		return;
	}
	scriptRef->setup(world->world_root_mut());
	cameraScriptRef->setup(world->world_root_mut());
}
