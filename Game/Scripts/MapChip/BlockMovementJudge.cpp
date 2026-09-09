#include "BlockMovementJudge.h"

#include <cmath>

namespace {

MapChipIndex Add(const MapChipIndex& lhs, const MapChipIndex& rhs) noexcept {
	return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

constexpr MapChipIndex kUp{ 0, 1, 0 };

/// プレイヤーが通れないチップか
bool IsSolid(MapChipType type) noexcept {
	return type == MapChipType::Clay || type == MapChipType::GoalPiece || type == MapChipType::GoalPieceUpper;
}

/// ワールド座標を含むセルの添字(セル中心が整数座標)
i32 CellIndex(float position) noexcept {
	return static_cast<i32>(std::floor(position + 0.5f));
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
std::optional<MapChipIndex> BlockMovementJudge::front_cell(
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
	return target;
}

std::optional<MapChipIndex> BlockMovementJudge::find_grip_target(
	const Vector3& playerPosition,
	const Vector3& playerDirection) const noexcept {
	const std::optional<MapChipIndex> front = front_cell(playerPosition, playerDirection);
	if (!front) {
		return std::nullopt;
	}
	const MapChipIndex target = *front;
	const MapChipType type = field_->get(target.x, target.y, target.z);
	if (type == MapChipType::GoalPieceUpper) {
		// 上段を掴んでも対象はピース本体(下段)
		return MapChipIndex{ target.x, target.y - 1, target.z };
	}
	if (type != MapChipType::GoalPiece && type != MapChipType::Clay) {
		return std::nullopt;
	}
	if (type == MapChipType::Clay && field_->is_clay_core(target)) {
		const MapChipIndex playerToClay = cardinal_direction(playerDirection);
		const MapChipIndex grippedFaceDirection{
			-playerToClay.x,
			0,
			-playerToClay.z,
		};
		// コアのプレイヤー側を向く面が伸長不可なら、その面から掴むこともできない。
		// 子Clayはコアの面設定に関係なくGripできる。
		if (field_->blocked_faces(target) & ClayFace::FromDirection(grippedFaceDirection)) {
			return std::nullopt;
		}
	}
	return target;
}

bool BlockMovementJudge::warn_blocked_grip(const Vector3& playerPosition, const Vector3& playerDirection) {
	const std::optional<MapChipIndex> front = front_cell(playerPosition, playerDirection);
	if (!front || !field_->is_clay_core(*front)) {
		return false;
	}
	const MapChipIndex playerToClay = cardinal_direction(playerDirection);
	const MapChipIndex grippedFaceDirection{ -playerToClay.x, 0, -playerToClay.z };
	if (!(field_->blocked_faces(*front) & ClayFace::FromDirection(grippedFaceDirection))) {
		return false;
	}
	field_->warn_blocked_face(*front, grippedFaceDirection);
	return true;
}

void BlockMovementJudge::warn_block_stuck(const MapChipIndex& blockIndex, const Vector3& playerDirection, BlockMoveDirection moveDirection) {
	if (field_) {
		field_->warn_block_stuck(blockIndex, relative_direction(playerDirection, moveDirection));
	}
}

bool BlockMovementJudge::is_clay(const MapChipIndex& index) const noexcept {
	return field_ && field_->get(index.x, index.y, index.z) == MapChipType::Clay;
}

bool BlockMovementJudge::is_goal_piece(const MapChipIndex& index) const noexcept {
	return field_ && field_->get(index.x, index.y, index.z) == MapChipType::GoalPiece;
}

bool BlockMovementJudge::is_visual_interpolating() const noexcept {
	return field_ && field_->is_visual_interpolating();
}

//===========================================
// AABBと重なるセルに固体があるか
//===========================================
bool BlockMovementJudge::overlaps_solid(const Vector3& min, const Vector3& max) const noexcept {
	// ステージ未ロード時は全域が範囲外になるので判定しない
	if (!field_ || field_->width() <= 0) {
		return false;
	}

	for (i32 z = CellIndex(min.z); z <= CellIndex(max.z); ++z) {
		for (i32 x = CellIndex(min.x); x <= CellIndex(max.x); ++x) {
			if (x < 0 || x >= field_->width() || z < 0 || z >= field_->depth()) {
				return true;
			}
			for (i32 y = CellIndex(min.y); y <= CellIndex(max.y); ++y) {
				if (IsSolid(field_->get(x, y, z))) {
					return true;
				}
			}
		}
	}
	return false;
}

//===========================================
// AABBと重なるセルの支えがゴール条件オブジェクトだけなら、中心に一番近いそのセル
//===========================================
std::optional<MapChipIndex> BlockMovementJudge::goal_piece_top_under(const Vector3& min, const Vector3& max) const noexcept {
	if (!field_ || field_->width() <= 0) {
		return std::nullopt;
	}

	const Vector3 center = (min + max) * 0.5f;
	std::optional<MapChipIndex> nearest;
	float nearestDistance = 0.0f;
	for (i32 z = CellIndex(min.z); z <= CellIndex(max.z); ++z) {
		for (i32 x = CellIndex(min.x); x <= CellIndex(max.x); ++x) {
			for (i32 y = CellIndex(min.y); y <= CellIndex(max.y); ++y) {
				const MapChipType type = field_->get(x, y, z);
				if (type == MapChipType::Clay) {
					return std::nullopt;
				}
				if (type != MapChipType::GoalPiece && type != MapChipType::GoalPieceUpper) {
					continue;
				}
				const float dx = center.x - static_cast<float>(x);
				const float dz = center.z - static_cast<float>(z);
				const float distance = dx * dx + dz * dz;
				if (!nearest || distance < nearestDistance) {
					nearest = MapChipIndex{ x, y, z };
					nearestDistance = distance;
				}
			}
		}
	}
	return nearest;
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

	// プレイヤーの正面がピースの下段か上段のどちらかであればよい
	const MapChipIndex forward = cardinal_direction(playerDirection);
	const MapChipIndex adjacent = Add(*playerIndex, forward);
	if (adjacent != blockIndex && adjacent != Add(blockIndex, kUp)) {
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
	BlockMoveDirection moveDirection,
	r32 visualMoveDuration) {
	if (!field_) {
		return std::nullopt;
	}

	std::optional<BlockMoveDestination> move =
		judge(playerPosition, blockIndex, playerDirection).destination(moveDirection);
	if (!move) {
		return std::nullopt;
	}
	const std::optional<MapChipIndex> landed =
		field_->move_goal_piece(blockIndex, move->blockIndex, visualMoveDuration);
	if (!landed) {
		return std::nullopt;
	}
	move->blockIndex = *landed;
	return move;
}

//===========================================
// Grip中の粘土を伸ばし、Playerと次のGrip対象を返す
//===========================================
std::optional<ClayDeformationResult> BlockMovementJudge::try_deform_clay(
	const Vector3& playerPosition,
	const MapChipIndex& clayIndex,
	const Vector3& playerDirection,
	BlockMoveDirection moveDirection,
	r32 visualMoveDuration) {
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
	if (!field_->stretch_clay(clayIndex, clayTo, visualMoveDuration)) {
		return std::nullopt;
	}
	return ClayDeformationResult{
		.playerIndex = playerTo,
		.clayIndex = clayTo,
		.type = clayDestination == MapChipType::GoalPiece || clayDestination == MapChipType::GoalPieceUpper
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

	// 移動先がピースと一緒に動くセル(下段・上段・つながった粘土)なら、同時移動で空くので入れる
	const MapChipIndex& playerTo = destination.playerIndex;
	const MapChipType playerToType = field_->get(playerTo.x, playerTo.y, playerTo.z);
	const bool vacated = field_->moves_with_goal_piece(source, playerTo);
	const bool canMovePlayer =
		field_->contains(playerTo) && (vacated || playerToType == MapChipType::Empty);
	if (!canMovePlayer || !field_->can_move_goal_piece(source, destination.blockIndex)) {
		return std::nullopt;
	}
	return destination;
}
