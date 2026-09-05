#pragma once
#include <memory>
#include <Engine/Runtime/Scene/Scene.h>

class FollowCamera;

class TitleScene : public szg::Scene {
public:
	TitleScene() noexcept;
	~TitleScene() noexcept override = default;

	SZG_CLASS_MOVE_ONLY(TitleScene)

public:

	void custom_load_asset() override;
	void custom_setup() override;

private:

	std::unique_ptr<FollowCamera> followCameraScript;
};

