#include "PlayerState.h"

#include "PlayerMovement.h"

bool IPlayerState::is_running() const noexcept {
	return isRunning_;
}

void PlayerIdleState::enter(PlayerContext&) {
	isRunning_ = true;
}

void PlayerIdleState::execute(PlayerContext&) {
}

void PlayerIdleState::exit(PlayerContext&) {
	isRunning_ = false;
}

void PlayerMoveState::enter(PlayerContext&) {
	isRunning_ = true;
}

void PlayerMoveState::execute(PlayerContext& context) {
	PlayerMovement::move_horizontal(context, context.moveSpeed);
}

void PlayerMoveState::exit(PlayerContext&) {
	isRunning_ = false;
}

void PlayerJumpState::enter(PlayerContext& context) {
	isRunning_ = context.worldInstance && context.isGrounded;
	verticalVelocity_ = isRunning_ ? context.jumpPower : 0.0f;
}

void PlayerJumpState::execute(PlayerContext& context) {
	if (!isRunning_ || !context.worldInstance) {
		isRunning_ = false;
		return;
	}

	// JumpStateを維持したまま横移動を行う
	PlayerMovement::move_horizontal(context, context.moveSpeed);

	verticalVelocity_ -= context.fallSpeed * context.deltaSeconds;
	auto& transform = context.worldInstance->transform_mut();
	auto position = transform.get_translate();
	position.y += verticalVelocity_ * context.deltaSeconds;

	if (verticalVelocity_ <= 0.0f && position.y <= 0.0f) {
		position.y = 0.0f;
		transform.set_translate(position);
		context.isGrounded = true;
		isRunning_ = false;
		return;
	}

	transform.set_translate(position);
	context.isGrounded = position.y == 0.0f;
}

void PlayerJumpState::exit(PlayerContext&) {
	verticalVelocity_ = 0.0f;
	isRunning_ = false;
}

void PlayerGripState::enter(PlayerContext& context) {
	isRunning_ = context.gripTargetIndex.has_value() && context.isGrounded;
	if (isRunning_) {
		context.grippedBlockIndex = context.gripTargetIndex;
	}
}

void PlayerGripState::execute(PlayerContext& context) {
	if (!isRunning_ || !context.grippedBlockIndex || !context.input.gripPressed) {
		isRunning_ = false;
		return;
	}

	// 掴み中は後退や横移動をしても、ブロックに向いた direction を維持する
	PlayerMovement::move_horizontal(context, context.gripMoveSpeed, false);
}

void PlayerGripState::exit(PlayerContext& context) {
	context.grippedBlockIndex.reset();
	context.blockMoveResult.reset();
	isRunning_ = false;
}
