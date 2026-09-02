#include "FactoryGJ26.h"

#include "MapTestScene.h"
#include "StageEditorScene.h"

std::unique_ptr<szg::Scene> FactoryGJ26::initialize_scene2() {
	return create_scene2(SceneListGJ26::MapTest);
}

std::unique_ptr<szg::Scene> FactoryGJ26::create_scene2(i32 next) {
	switch (next) {
	case SceneListGJ26::MapTest:
	default:
		return std::make_unique<MapTestScene>();

	case SceneListGJ26::StageEditor:
		return std::make_unique<StageEditorScene>();
	}
}
