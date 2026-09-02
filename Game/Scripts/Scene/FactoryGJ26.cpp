#include "FactoryGJ26.h"

#include <Engine/Runtime/Scene/Scene.h>
#include "PlayerDevScene.h"

std::unique_ptr<szg::Scene> FactoryGJ26::initialize_scene2() {
	return create_scene2(0);
}

std::unique_ptr<szg::Scene> FactoryGJ26::create_scene2(i32) {
	return std::make_unique<PlayerDevScene>();
}



