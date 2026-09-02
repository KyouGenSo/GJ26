#pragma once

#include <Engine/Runtime/Scene/Scene.h>

/// <summary>
/// マップチップの表示確認用シーン
/// </summary>
class MapTestScene final : public szg::Scene {
public:
	MapTestScene();

public:
	void custom_load_asset() override;
	void custom_setup() override;
};
