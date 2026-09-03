#include "PlayerState.h"

#include <Engine/Application/Logger.h>

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
		const MapChipIndex& index = *context.grippedBlockIndex;
		szgInformation(
			"Player: grabbed block. index=({}, {}, {})",
			index.x, index.y, index.z);
	}
}

void PlayerGripState::execute(PlayerContext& context) {
	if (!isRunning_ || !context.grippedBlockIndex || !context.input.gripPressed) {
		isRunning_ = false;
		return;
	}

	// Grip中の移動はPlayerがGoalPieceと同時にグリッド単位で処理する
}

void PlayerGripState::exit(PlayerContext& context) {
	if (context.grippedBlockIndex) {
		const MapChipIndex& index = *context.grippedBlockIndex;
		szgInformation(
			"Player: released block. index=({}, {}, {})",
			index.x, index.y, index.z);
	}
	context.grippedBlockIndex.reset();
	context.blockMoveResult.reset();
	isRunning_ = false;
}
