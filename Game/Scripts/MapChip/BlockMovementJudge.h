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
/// 掴んだブロックを移動できる隣接マス
/// nullopt の方向には移動できない
/// </summary>
struct BlockMoveResult {
	MapChipIndex blockIndex;
	std::optional<MapChipIndex> forward;
	std::optional<MapChipIndex> backward;
	std::optional<MapChipIndex> left;
	std::optional<MapChipIndex> right;

	/// <summary>
	/// 指定方向に移動できるか
	/// </summary>
	/// <param name="direction"></param>
	/// <returns></returns>
	bool can_move(BlockMoveDirection direction) const noexcept;
	std::optional<MapChipIndex> destination(BlockMoveDirection direction) const noexcept;
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
	explicit BlockMovementJudge(Reference<const MapChipField> field) noexcept;

	/// <summary>
	/// MapChipField の設定
	/// </summary>
	/// <param name="field"></param>
	void set_field(Reference<const MapChipField> field) noexcept;

	/// プレイヤーの現在マスに隣接する、向いている方向のブロックを取得
	std::optional<MapChipIndex> find_grip_target(
		const Vector3& playerPosition,
		const Vector3& playerDirection) const noexcept;

	/// 掴んだブロックがプレイヤー基準の前後左右へ移動できるかを取得
	BlockMoveResult judge(
		const MapChipIndex& blockIndex,
		const Vector3& playerDirection) const noexcept;

private:

	/// プレイヤーの向きベクトルから、前方方向のグリッド座標オフセットを取得
	static MapChipIndex cardinal_direction(const Vector3& direction) noexcept;
	std::optional<MapChipIndex> find_empty_destination(
		const MapChipIndex& source,
		const MapChipIndex& offset) const noexcept;

private:
	Reference<const MapChipField> field_;
};
