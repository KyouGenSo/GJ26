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
	// 支えの無いセルへ移動して落下し始めたら掴みを離す
	if (!isRunning_ || !context.grippedBlockIndex || !context.input.gripPressed || !context.isGrounded) {
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
