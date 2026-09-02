#pragma once

#include <Engine/Runtime/Scene/Scene.h>

#include "Scripts/MapChip/MapChipField.h"

/// <summary>
/// マップチップの表示確認用シーン
/// </summary>
class MapTestScene final : public szg::Scene {
public:
	MapTestScene();

public:
	void custom_setup() override;

private:
	MapChipField field;
};
