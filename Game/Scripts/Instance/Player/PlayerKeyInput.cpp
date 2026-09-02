#include "PlayerKeyInput.h"

#include <algorithm>

PlayerKeyInput::PlayerKeyInput() {
	mapper.bind(PlayerInputAction::Jump, szg::KeyID::Space);
}

PlayerInputFrame PlayerKeyInput::update() {
	mapper.update();

	PlayerInputFrame result;
	result.move = szg::InputAdvanced::PressWASD();
	const Vector2 mouseDelta = szg::Input::MouseDelta();
	result.cameraRotationDelta = {
		mouseDelta.x * mouseSensitivity_,
		-mouseDelta.y * mouseSensitivity_,
	};
	result.jumpTriggered = mapper.trigger(PlayerInputAction::Jump);
	result.pushPressed = mapper.press(PlayerInputAction::Push);
	result.pullPressed = mapper.press(PlayerInputAction::Pull);
	return result;
}

InputMapper<PlayerInputAction, szg::KeyID>& PlayerKeyInput::mapper_mut() noexcept {
	return mapper;
}

float PlayerKeyInput::get_mouse_sensitivity() const noexcept {
	return mouseSensitivity_;
}

void PlayerKeyInput::set_mouse_sensitivity(float mouseSensitivity) noexcept {
	mouseSensitivity_ = std::max(mouseSensitivity, 0.0f);
}
