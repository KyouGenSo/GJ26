#pragma once

#include <Engine/Runtime/Scene/Scene.h>

/// <summary>
/// ステージ選択画面
/// </summary>
class SelectScene final : public szg::Scene {
public:
	SelectScene() noexcept;
	~SelectScene() noexcept override = default;

	SZG_CLASS_MOVE_ONLY(SelectScene)

public:
	void custom_load_asset() override;
	void custom_setup() override;
};
