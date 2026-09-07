#include "PlayerMoveState.h"
#include "Scripts/Instance/Player/PlayerMovement.h"

void PlayerMoveState::enter(PlayerContext&) {
	isRunning_ = true;
}

void PlayerMoveState::execute(PlayerContext& context) {
	PlayerMovement::move_horizontal(context, context.moveSpeed);
}

void PlayerMoveState::exit(PlayerContext&) {
	isRunning_ = false;
}