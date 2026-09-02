#include "GJ26.h"

#include <Engine/Runtime/Scene/SceneManager2.h>

#include "./Scene/FactoryGJ26.h"

void GJ26::initialize() {
	szg::SceneManager2::SetupFactory(std::make_unique<FactoryGJ26>());
}