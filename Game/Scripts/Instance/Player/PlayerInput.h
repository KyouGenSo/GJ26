#pragma once

#include "PlayerInputFrame.h"
#include "PlayerKeyInput.h"
#include "PlayerPadInput.h"

/// <summary>
/// プレイヤー入力の統合処理
/// </summary>
class PlayerInput {
public:
	PlayerInputFrame update();

	PlayerKeyInput& key_input_mut() noexcept;
	PlayerPadInput& pad_input_mut() noexcept;

private:
	PlayerKeyInput keyInput;
	PlayerPadInput padInput;
};
