#include "PlayerPadInput.h"

PlayerPadInput::PlayerPadInput() {
	mapper.bind(PlayerInputAction::Jump, szg::PadID::A);
	mapper.bind(PlayerInputAction::Grip, szg::PadID::B);
}

PlayerInputFrame PlayerPadInput::update() {
	mapper.update();

	PlayerInputFrame result;
	result.move = szg::Input::StickL();
	result.cameraRotationInput = szg::Input::StickR();
	result.jumpTriggered = mapper.trigger(PlayerInputAction::Jump);
	result.gripPressed = mapper.press(PlayerInputAction::Grip);
	return result;
}

InputMapper<PlayerInputAction, szg::PadID>& PlayerPadInput::mapper_mut() noexcept {
	return mapper;
}
