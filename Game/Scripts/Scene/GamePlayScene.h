#pragma once
#include <Engine/Runtime/Scene/Scene.h>

class GamePlayScene : public szg::Scene {
public:

	GamePlayScene() noexcept;
	~GamePlayScene() noexcept override;

	SZG_CLASS_MOVE_ONLY(GamePlayScene)

public:
	void custom_load_asset() override;
	void custom_setup() override;

};
