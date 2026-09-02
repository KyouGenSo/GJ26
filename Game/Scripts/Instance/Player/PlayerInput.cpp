#include "PlayerInput.h"

PlayerInputFrame PlayerInput::update() {
	const PlayerInputFrame key = keyInput.update();
	const PlayerInputFrame pad = padInput.update();

	PlayerInputFrame result;
	result.move = key.move.length() >= pad.move.length() ? key.move : pad.move;
	if (result.move.length() > 1.0f) {
		result.move = result.move.normalize();
	}

	result.jumpTriggered = key.jumpTriggered || pad.jumpTriggered;
	result.pushPressed = key.pushPressed || pad.pushPressed;
	result.pullPressed = key.pullPressed || pad.pullPressed;
	return result;
}

PlayerKeyInput& PlayerInput::key_input_mut() noexcept {
	return keyInput;
}

PlayerPadInput& PlayerInput::pad_input_mut() noexcept {
	return padInput;
}
