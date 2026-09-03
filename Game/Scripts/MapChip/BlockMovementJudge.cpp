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

std::optional<MapChipIndex> BlockMoveResult::destination(BlockMoveDirection direction) const noexcept {
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
BlockMovementJudge::BlockMovementJudge(Reference<const MapChipField> field) noexcept
	: field_(field) {
}

//===========================================
// MapChipField の設定
//===========================================
void BlockMovementJudge::set_field(Reference<const MapChipField> field) noexcept {
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
	if (!field_->contains(target) || field_->get(target.x, target.y, target.z) == MapChipType::Empty) {
		return std::nullopt;
	}
	return target;
}

//===========================================
// 掴んだブロックがプレイヤー基準の前後左右へ移動できるかを取得
//===========================================
BlockMoveResult BlockMovementJudge::judge(
	const MapChipIndex& blockIndex,
	const Vector3& playerDirection) const noexcept {
	BlockMoveResult result{};
	result.blockIndex = blockIndex;

	if (!field_ || !field_->contains(blockIndex) ||
		field_->get(blockIndex.x, blockIndex.y, blockIndex.z) == MapChipType::Empty) {
		return result;
	}

	const MapChipIndex forward = cardinal_direction(playerDirection);
	const MapChipIndex backward{ -forward.x, 0, -forward.z };
	const MapChipIndex right{ forward.z, 0, -forward.x };
	const MapChipIndex left{ -right.x, 0, -right.z };

	result.forward = find_empty_destination(blockIndex, forward);
	result.backward = find_empty_destination(blockIndex, backward);
	result.left = find_empty_destination(blockIndex, left);
	result.right = find_empty_destination(blockIndex, right);
	return result;
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

//===========================================
// 指定マスから指定オフセット先のマスが空かどうかを取得
//===========================================
std::optional<MapChipIndex> BlockMovementJudge::find_empty_destination(
	const MapChipIndex& source,
	const MapChipIndex& offset) const noexcept {
	const MapChipIndex destination = Add(source, offset);
	if (!field_->contains(destination) ||
		field_->get(destination.x, destination.y, destination.z) != MapChipType::Empty) {
		return std::nullopt;
	}
	return destination;
}
