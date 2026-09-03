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
	InputMapper<PlayerInputAction, szg::MouseID>& mouse_mapper_mut() noexcept;
	float get_mouse_sensitivity() const noexcept;
	void set_mouse_sensitivity(float mouseSensitivity) noexcept;

private:
	InputMapper<PlayerInputAction, szg::KeyID> mapper;
	InputMapper<PlayerInputAction, szg::MouseID> mouseMapper;
	float mouseSensitivity_{ 0.002f };
};
