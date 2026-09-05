#include "PlayerKeyInput.h"

#include <algorithm>

PlayerKeyInput::PlayerKeyInput() {
	mapper.bind(PlayerInputAction::Jump, szg::KeyID::Space);
	mouseMapper.bind(PlayerInputAction::Grip, szg::MouseID::Left);
}

PlayerInputFrame PlayerKeyInput::update() {
	mapper.update();
	mouseMapper.update();

	PlayerInputFrame result;
	result.move = szg::InputAdvanced::PressWASD();
	if (szg::Input::IsPressMouse(szg::MouseID::Right)) {
		const Vector2 mouseDelta = szg::Input::MouseDelta();
		result.cameraRotationDelta = {
			mouseDelta.x * mouseSensitivity_,
			-mouseDelta.y * mouseSensitivity_,
		};
	}
	result.jumpTriggered = mapper.trigger(PlayerInputAction::Jump);
	result.gripPressed = mouseMapper.press(PlayerInputAction::Grip);
	return result;
}

InputMapper<PlayerInputAction, szg::KeyID>& PlayerKeyInput::mapper_mut() noexcept {
	return mapper;
}

InputMapper<PlayerInputAction, szg::MouseID>& PlayerKeyInput::mouse_mapper_mut() noexcept {
	return mouseMapper;
}

float PlayerKeyInput::get_mouse_sensitivity() const noexcept {
	return mouseSensitivity_;
}

void PlayerKeyInput::set_mouse_sensitivity(float mouseSensitivity) noexcept {
	mouseSensitivity_ = std::max(mouseSensitivity, 0.0f);
}
