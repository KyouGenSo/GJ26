#pragma once

#include <Engine/Runtime/SceneScript/ISceneScript.h>

#include <Library/Utility/Template/Reference.h>

#include <Engine/Module/World/WorldInstance/WorldInstance.h>

class AxisController final : public szg::ISceneScript {
public:
	AxisController() = default;
	~AxisController() noexcept override = default;

public:
	void setup();

	void post_update() override;

private:
	Reference<szg::WorldInstance> axisInstance;
};
