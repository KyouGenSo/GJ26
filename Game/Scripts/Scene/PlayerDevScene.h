#pragma once
#include <memory>

#include <Engine/Runtime/Scene/Scene.h>

class Player;
class FollowCamera;

class PlayerDevScene : public szg::Scene {
public:

	PlayerDevScene() noexcept;
	~PlayerDevScene() noexcept override;

	SZG_CLASS_MOVE_ONLY(PlayerDevScene)

public:
	void custom_load_asset() override;
	void custom_setup() override;

private:

	std::unique_ptr<Player> playerScript;
	std::unique_ptr<FollowCamera> followCameraScript;

};
