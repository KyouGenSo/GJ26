#include "BlockMovementJudge.h"

#include <cmath>

namespace {

MapChipIndex Add(const MapChipIndex& lhs, const MapChipIndex& rhs) noexcept {
	return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

} // namespace

//===========================================
// 指定方向に移動できるか
//===========================================
bool BlockMoveResult::can_move(BlockMoveDirection direction) const noexcept {
	return destination(direction).has_value();
}

std::optional<BlockMoveDestination> BlockMoveResult::destination(BlockMoveDirection direction) const noexcept {
	switch (direction) {
	case BlockMoveDirection::Forward:
		return forward;
	case BlockMoveDirection::Backward:
		return backward;
	case BlockMoveDirection::Left:
		return left;
	case BlockMoveDirection::Right:
		return right;
	default:
		return std::nullopt;
	}
}

//===========================================
// コンストラクタ
//===========================================
BlockMovementJudge::BlockMovementJudge(Reference<MapChipField> field) noexcept
	: field_(field) {
}

//===========================================
// MapChipField の設定
//===========================================
void BlockMovementJudge::set_field(Reference<MapChipField> field) noexcept {
	field_ = field;
}

//===========================================
// プレイヤーの現在マスに隣接する、向いている方向のブロックを取得
//===========================================
std::optional<MapChipIndex> BlockMovementJudge::find_grip_target(
	const Vector3& playerPosition,
	const Vector3& playerDirection) const noexcept {
	if (!field_) {
		return std::nullopt;
	}

	const std::optional<MapChipIndex> playerIndex = field_->to_index(playerPosition);
	if (!playerIndex) {
		return std::nullopt;
	}

	const MapChipIndex target = Add(*playerIndex, cardinal_direction(playerDirection));
	if (!field_->contains(target)) {
		return std::nullopt;
	}
	const MapChipType type = field_->get(target.x, target.y, target.z);
	if (type != MapChipType::GoalPiece && type != MapChipType::Clay) {
		return std::nullopt;
	}
	return target;
}

bool BlockMovementJudge::is_clay(const MapChipIndex& index) const noexcept {
	return field_ && field_->get(index.x, index.y, index.z) == MapChipType::Clay;
}

bool BlockMovementJudge::is_goal_piece(const MapChipIndex& index) const noexcept {
	return field_ && field_->get(index.x, index.y, index.z) == MapChipType::GoalPiece;
}

//===========================================
// 掴んだブロックがプレイヤー基準の前後左右へ移動できるかを取得
//===========================================
BlockMoveResult BlockMovementJudge::judge(
	const Vector3& playerPosition,
	const MapChipIndex& blockIndex,
	const Vector3& playerDirection) const noexcept {
	BlockMoveResult result{};
	result.blockIndex = blockIndex;

	if (!field_) {
		return result;
	}

	const std::optional<MapChipIndex> playerIndex = field_->to_index(playerPosition);
	if (!playerIndex || !field_->contains(blockIndex) ||
		field_->get(blockIndex.x, blockIndex.y, blockIndex.z) != MapChipType::GoalPiece) {
		return result;
	}

	const MapChipIndex forward = cardinal_direction(playerDirection);
	if (Add(*playerIndex, forward) != blockIndex) {
		return result;
	}
	result.playerIndex = *playerIndex;

	const MapChipIndex backward{ -forward.x, 0, -forward.z };
	const MapChipIndex right{ forward.z, 0, -forward.x };
	const MapChipIndex left{ -right.x, 0, -right.z };

	result.forward = find_goal_piece_destination(*playerIndex, blockIndex, forward);
	result.backward = find_goal_piece_destination(*playerIndex, blockIndex, backward);
	result.left = find_goal_piece_destination(*playerIndex, blockIndex, left);
	result.right = find_goal_piece_destination(*playerIndex, blockIndex, right);
	return result;
}

//===========================================
// 判定に成功した場合だけGoalPieceを移動する
//===========================================
std::optional<BlockMoveDestination> BlockMovementJudge::try_move_goal_piece(
	const Vector3& playerPosition,
	const MapChipIndex& blockIndex,
	const Vector3& playerDirection,
	BlockMoveDirection moveDirection) {
	if (!field_) {
		return std::nullopt;
	}

	const std::optional<BlockMoveDestination> move =
		judge(playerPosition, blockIndex, playerDirection).destination(moveDirection);
	if (!move || !field_->move_goal_piece(blockIndex, move->blockIndex)) {
		return std::nullopt;
	}
	return move;
}

//===========================================
// Grip中の粘土を伸ばし、Playerと次のGrip対象を返す
//===========================================
std::optional<ClayDeformationResult> BlockMovementJudge::try_deform_clay(
	const Vector3& playerPosition,
	const MapChipIndex& clayIndex,
	const Vector3& playerDirection,
	BlockMoveDirection moveDirection) {
	if (!field_ || !is_clay(clayIndex)) {
		return std::nullopt;
	}

	const std::optional<MapChipIndex> playerIndex = field_->to_index(playerPosition);
	if (!playerIndex || Add(*playerIndex, cardinal_direction(playerDirection)) != clayIndex) {
		return std::nullopt;
	}

	const MapChipIndex offset = relative_direction(playerDirection, moveDirection);
	const MapChipIndex playerTo = Add(*playerIndex, offset);
	const MapChipIndex clayTo = Add(clayIndex, offset);
	if (!field_->contains(playerTo) || !field_->contains(clayTo)) {
		return std::nullopt;
	}

	const MapChipType playerDestination = field_->get(playerTo.x, playerTo.y, playerTo.z);
	if (playerTo == clayIndex ||
		(playerDestination != MapChipType::Empty && playerDestination != MapChipType::Goal)) {
		return std::nullopt;
	}

	const MapChipType clayDestination = field_->get(clayTo.x, clayTo.y, clayTo.z);
	if (!field_->stretch_clay(clayIndex, clayTo)) {
		return std::nullopt;
	}
	return ClayDeformationResult{
		.playerIndex = playerTo,
		.clayIndex = clayTo,
		.type = clayDestination == MapChipType::GoalPiece
			? ClayDeformationType::Connect
			: ClayDeformationType::Stretch,
	};
}

//===========================================
// プレイヤーの向きベクトルから、前方方向のグリッド座標オフセットを取得
//===========================================
MapChipIndex BlockMovementJudge::cardinal_direction(const Vector3& direction) noexcept {
	if (std::abs(direction.x) > std::abs(direction.z)) {
		return { direction.x < 0.0f ? -1 : 1, 0, 0 };
	}
	return { 0, 0, direction.z < 0.0f ? -1 : 1 };
}

MapChipIndex BlockMovementJudge::relative_direction(
	const Vector3& playerDirection,
	BlockMoveDirection moveDirection) noexcept {
	const MapChipIndex forward = cardinal_direction(playerDirection);
	const MapChipIndex right{ forward.z, 0, -forward.x };
	switch (moveDirection) {
	case BlockMoveDirection::Forward:
		return forward;
	case BlockMoveDirection::Backward:
		return { -forward.x, 0, -forward.z };
	case BlockMoveDirection::Left:
		return { -right.x, 0, -right.z };
	case BlockMoveDirection::Right:
		return right;
	default:
		return {};
	}
}

//===========================================
// GoalPieceを指定マスから指定オフセット先へ動かせるかどうかを取得
//===========================================
std::optional<BlockMoveDestination> BlockMovementJudge::find_goal_piece_destination(
	const MapChipIndex& playerIndex,
	const MapChipIndex& source,
	const MapChipIndex& offset) const noexcept {
	const BlockMoveDestination destination{
		.playerIndex = Add(playerIndex, offset),
		.blockIndex = Add(source, offset),
	};

	// 前進時はPlayerがGoalPieceの元セルへ入るため、同時移動によって空くセルとして扱う
	const bool canMovePlayer =
		field_->contains(destination.playerIndex) &&
		(destination.playerIndex == source ||
			field_->get(
				destination.playerIndex.x,
				destination.playerIndex.y,
				destination.playerIndex.z) == MapChipType::Empty);
	if (!canMovePlayer || !field_->can_move_goal_piece(source, destination.blockIndex)) {
		return std::nullopt;
	}
	return destination;
}
