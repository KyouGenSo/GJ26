#pragma once

#include <optional>

#include <Library/Math/Vector3.h>
#include <Library/Utility/Template/Reference.h>

#include "MapChipField.h"

/// <summary>
/// プレイヤーの向きを基準にしたブロックの移動方向
/// </summary>
enum class BlockMoveDirection {
	Forward,
	Backward,
	Left,
	Right,
};

/// <summary>
/// 1回のグリッド移動後のPlayerとGoalPieceの位置
/// </summary>
struct BlockMoveDestination {
	MapChipIndex playerIndex{};
	MapChipIndex blockIndex{};
};

/// <summary>
/// 掴んだGoalPieceを移動できる隣接マス
/// nullopt の方向には移動できない
/// </summary>
struct BlockMoveResult {
	MapChipIndex playerIndex{};
	MapChipIndex blockIndex{};
	std::optional<BlockMoveDestination> forward;
	std::optional<BlockMoveDestination> backward;
	std::optional<BlockMoveDestination> left;
	std::optional<BlockMoveDestination> right;

	/// <summary>
	/// 指定方向に移動できるか
	/// </summary>
	/// <param name="direction"></param>
	/// <returns></returns>
	bool can_move(BlockMoveDirection direction) const noexcept;
	std::optional<BlockMoveDestination> destination(BlockMoveDirection direction) const noexcept;
};

/// <summary>
/// MapChipField の状態から、ブロックの選択と前後左右への移動可否を判定する
/// Player と MapChipField を直接結合しないための仲介クラス
/// </summary>
class BlockMovementJudge {
public:

	/// <summary>
	/// コンストラクタ・デストラクタ
	/// </summary>
	BlockMovementJudge() = default;
	explicit BlockMovementJudge(Reference<MapChipField> field) noexcept;

	/// <summary>
	/// MapChipField の設定
	/// </summary>
	/// <param name="field"></param>
	void set_field(Reference<MapChipField> field) noexcept;

	/// プレイヤーの現在マスに隣接する、向いている方向のブロックを取得
	std::optional<MapChipIndex> find_grip_target(
		const Vector3& playerPosition,
		const Vector3& playerDirection) const noexcept;

	/// 掴んだブロックがプレイヤー基準の前後左右へ移動できるかを取得
	BlockMoveResult judge(
		const Vector3& playerPosition,
		const MapChipIndex& blockIndex,
		const Vector3& playerDirection) const noexcept;

	/// 判定に成功した場合だけGoalPieceを移動し、PlayerとGoalPieceの移動先を返す
	std::optional<BlockMoveDestination> try_move_goal_piece(
		const Vector3& playerPosition,
		const MapChipIndex& blockIndex,
		const Vector3& playerDirection,
		BlockMoveDirection moveDirection);

private:

	/// プレイヤーの向きベクトルから、前方方向のグリッド座標オフセットを取得
	static MapChipIndex cardinal_direction(const Vector3& direction) noexcept;
	std::optional<BlockMoveDestination> find_goal_piece_destination(
		const MapChipIndex& playerIndex,
		const MapChipIndex& source,
		const MapChipIndex& offset) const noexcept;

private:
	Reference<MapChipField> field_;
};
