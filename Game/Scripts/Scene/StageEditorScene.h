#pragma once

#include <Engine/Runtime/Scene/Scene.h>

/// <summary>
/// ステージエディターの3Dプレビュー専用シーン
/// </summary>
class StageEditorScene final : public szg::Scene {
public:
	StageEditorScene();

public:
	void custom_load_asset() override;
	void custom_setup() override;
};
