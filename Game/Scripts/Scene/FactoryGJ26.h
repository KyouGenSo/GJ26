#pragma once

#include <Engine/Runtime/Scene/BaseSceneFactory.h>

enum SceneListGJ26 {
	MapTest = 0,
	StageEditor,
	PlayerDev,
	Title,
	Select,

	NumScene,
};

class FactoryGJ26 final : public szg::BaseSceneFactory {

public:
	std::unique_ptr<szg::Scene> create_scene2(i32 next) const override;
};
