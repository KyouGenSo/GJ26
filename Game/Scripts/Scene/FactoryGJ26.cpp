#include "FactoryGJ26.h"

#include "MapTestScene.h"
#include "StageEditorScene.h"
#include "PlayerDevScene.h"

std::unique_ptr<szg::Scene> FactoryGJ26::create_scene2(i32 next) const {
	switch (next) {
	case SceneListGJ26::MapTest:
		return std::make_unique<MapTestScene>();

	case SceneListGJ26::StageEditor:
		return std::make_unique<StageEditorScene>();

	case SceneListGJ26::PlayerDev:
		return std::make_unique<PlayerDevScene>();

	default:
		return nullptr;
	}
}
