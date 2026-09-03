#include "GJ26.h"

#include <Engine/Runtime/Scene/SceneManager2.h>

#include "./Scene/FactoryGJ26.h"

#ifdef DEBUG_FEATURES_ENABLE
#include <Engine/Debug/Editor/Core/CustomEditor/CustomEditorManager.h>
#include <Engine/Debug/Editor/EditorMain.h>
#include "Editor/StageEditorWindow.h"
#endif

void GJ26::initialize() {
	szg::SceneManager2::SetupFactory(std::make_unique<FactoryGJ26>());

#ifdef DEBUG_FEATURES_ENABLE
	auto manager = std::make_unique<szg::CustomEditorManager>();
	manager->register_editor_window(
		"ステージエディタ",
		std::make_unique<StageEditorWindow>()
	);
	szg::EditorMain::SetCustomEditorManager(std::move(manager));
#endif
}