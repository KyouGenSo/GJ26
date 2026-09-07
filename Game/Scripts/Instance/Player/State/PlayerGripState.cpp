#include "PlayerGripState.h"


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