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
	szg::SceneManager2::SetupInitialScene(SceneListGJ26::Title);

	// 共通視覚アセット(Cube / Clay / Goal / 全ステージの粘土ブロック OBJ)は
	// TitleScript::setup() で RegisterVisualAssets() を呼んでロード開始する。
	// 完了まではタイトルシーンのローディング演出で覆う

#ifdef DEBUG_FEATURES_ENABLE
	auto manager = std::make_unique<szg::CustomEditorManager>();
	manager->register_editor_window(
		"ステージエディタ",
		std::make_unique<StageEditorWindow>()
	);
	szg::EditorMain::SetCustomEditorManager(std::move(manager));
#endif
}
