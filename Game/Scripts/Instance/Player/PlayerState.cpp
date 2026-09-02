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
	isRunning_ = context.canGrip && context.isGrounded;
}

void PlayerGripState::execute(PlayerContext& context) {
	if (!isRunning_ || !context.canGrip || !is_grip_input_active(context)) {
		isRunning_ = false;
		return;
	}

	PlayerMovement::move_horizontal(context, context.gripMoveSpeed);
	execute_grip(context);
}

void PlayerGripState::exit(PlayerContext& context) {
	exit_grip(context);
	isRunning_ = false;
}

void PlayerGripState::exit_grip(PlayerContext&) {
}

bool PlayerPushState::is_grip_input_active(const PlayerContext& context) const noexcept {
	return context.input.pushPressed;
}

void PlayerPushState::execute_grip(PlayerContext&) {
	// Push対象の移動処理は対象選択の仕様決定後に実装する
}

bool PlayerPullState::is_grip_input_active(const PlayerContext& context) const noexcept {
	return context.input.pullPressed;
}

void PlayerPullState::execute_grip(PlayerContext&) {
	// Pull対象の移動処理は対象選択の仕様決定後に実装する
}
