#pragma once

#include <Engine/Runtime/Input/Input.h>

#include <Scripts/InputMapper.h>

#include "PlayerInputFrame.h"

/// <summary>
/// プレイヤーのゲームパッド入力
/// </summary>
class PlayerPadInput {
public:
	PlayerPadInput();

	PlayerInputFrame update();

	InputMapper<PlayerInputAction, szg::PadID>& mapper_mut() noexcept;

private:
	InputMapper<PlayerInputAction, szg::PadID> mapper;
};
