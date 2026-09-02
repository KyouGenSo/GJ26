#pragma once

#include <Engine/Runtime/Input/Input.h>

#include <Scripts/InputMapper.h>

#include "PlayerInputFrame.h"

/// <summary>
/// プレイヤーのキーボード入力
/// </summary>
class PlayerKeyInput {
public:
	PlayerKeyInput();

	PlayerInputFrame update();

	InputMapper<PlayerInputAction, szg::KeyID>& mapper_mut() noexcept;

private:
	InputMapper<PlayerInputAction, szg::KeyID> mapper;
};
