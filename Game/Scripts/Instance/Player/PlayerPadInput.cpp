#include "PlayerPadInput.h"

PlayerPadInput::PlayerPadInput() {
	mapper.bind(PlayerInputAction::Jump, szg::PadID::A);
}

PlayerInputFrame PlayerPadInput::update() {
	mapper.update();

	PlayerInputFrame result;
	result.move = szg::Input::StickL();
	result.cameraRotationInput = szg::Input::StickR();
	result.jumpTriggered = mapper.trigger(PlayerInputAction::Jump);
	result.pushPressed = mapper.press(PlayerInputAction::Push);
	result.pullPressed = mapper.press(PlayerInputAction::Pull);
	return result;
}

InputMapper<PlayerInputAction, szg::PadID>& PlayerPadInput::mapper_mut() noexcept {
	return mapper;
}
