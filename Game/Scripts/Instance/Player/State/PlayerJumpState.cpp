#include "PlayerJumpState.h"
#include "Scripts/Instance/Player/PlayerMovement.h"

void PlayerJumpState::enter(PlayerContext& context) {
	isRunning_ = context.worldInstance && context.isGrounded;
	if (isRunning_) {
		context.verticalVelocity = context.jumpPower;
	}
}

void PlayerJumpState::execute(PlayerContext& context) {
	// Yの更新と着地判定はPlayerMovement::apply_gravityが行う。上昇が終わって接地したら終了
	if (!isRunning_ || !context.worldInstance ||
		(context.isGrounded && context.verticalVelocity <= 0.0f)) {
		isRunning_ = false;
		return;
	}

	// JumpStateを維持したまま横移動を行う
	PlayerMovement::move_horizontal(context, context.moveSpeed);
}

void PlayerJumpState::exit(PlayerContext&) {
	isRunning_ = false;
}