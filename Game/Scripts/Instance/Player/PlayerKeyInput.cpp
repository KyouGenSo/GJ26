#include "PlayerKeyInput.h"

PlayerKeyInput::PlayerKeyInput() {
	mapper.bind(PlayerInputAction::Jump, szg::KeyID::Space);
}

PlayerInputFrame PlayerKeyInput::update() {
	mapper.update();

	PlayerInputFrame result;
	result.move = szg::InputAdvanced::PressWASD();
	result.jumpTriggered = mapper.trigger(PlayerInputAction::Jump);
	result.pushPressed = mapper.press(PlayerInputAction::Push);
	result.pullPressed = mapper.press(PlayerInputAction::Pull);
	return result;
}

InputMapper<PlayerInputAction, szg::KeyID>& PlayerKeyInput::mapper_mut() noexcept {
	return mapper;
}
