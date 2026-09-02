#pragma once
#include <Engine/Runtime/Scene/Scene.h>


class PlayerDevScene : public szg::Scene {
public:

	PlayerDevScene() noexcept;
	~PlayerDevScene() noexcept override = default;

	SZG_CLASS_MOVE_ONLY(PlayerDevScene)

public:
	void custom_setup() override;

private:


};