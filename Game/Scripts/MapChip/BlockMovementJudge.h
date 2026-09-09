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

enum class ClayDeformationType {
	Stretch,
	Connect,
};

/// <summary>
/// 1回の粘土変形後のPlayerとGrip対象
/// </summary>
struct ClayDeformationResult {
	MapChipIndex playerIndex{};
	MapChipIndex clayIndex{};
	ClayDeformationType type{ ClayDeformationType::Stretch };
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

	/// プレイヤーの現在マスに隣接する、向いている方向のブロックを取得(ゴール条件オブジェクトの上段なら下段の index)
	std::optional<MapChipIndex> find_grip_target(
		const Vector3& playerPosition,
		const Vector3& playerDirection) const noexcept;

	/// <summary>
	/// 向いている先が粘土のコアで、プレイヤー側の面が塞がれていれば、その面の cross の演出を始める(掴み拒否の通知)
	/// </summary>
	/// <returns>演出を始めたら true</returns>
	bool warn_blocked_grip(const Vector3& playerPosition, const Vector3& playerDirection);

	/// <summary>
	/// 掴んだブロックを moveDirection へ動かせなかったとき、そのブロック全体を動かそうとした方向に振動させる
	/// </summary>
	void warn_block_stuck(const MapChipIndex& blockIndex, const Vector3& playerDirection, BlockMoveDirection moveDirection);

	/// 指定セルが粘土か
	bool is_clay(const MapChipIndex& index) const noexcept;

	/// 指定セルがゴール条件オブジェクトか
	bool is_goal_piece(const MapChipIndex& index) const noexcept;

	/// MapChipField の表示移動補間(押し・落下)が再生中か
	bool is_visual_interpolating() const noexcept;

	/// <summary>
	/// <para>ワールド座標のAABB [min, max] と重なるセルに固体(粘土・ゴール条件オブジェクトの下段と上段)があるか</para>
	/// <para>ステージのXZ範囲外は固体(見えない壁)。Yの範囲外はEmpty(上へは飛べる。下は地面が受ける)</para>
	/// </summary>
	bool overlaps_solid(const Vector3& min, const Vector3& max) const noexcept;

	/// <summary>
	/// <para>ワールド座標のAABB [min, max] と重なるセルの支えがゴール条件オブジェクト(下段・上段)だけなら、AABB 中心に一番近いそのセル</para>
	/// <para>粘土が 1 つでも重なっていれば nullopt(粘土に乗っている扱い)</para>
	/// </summary>
	std::optional<MapChipIndex> goal_piece_top_under(const Vector3& min, const Vector3& max) const noexcept;

	/// 掴んだブロックがプレイヤー基準の前後左右へ移動できるかを取得
	BlockMoveResult judge(
		const Vector3& playerPosition,
		const MapChipIndex& blockIndex,
		const Vector3& playerDirection) const noexcept;

	/// 判定に成功した場合だけGoalPieceを移動し、PlayerとGoalPieceの移動先を返す(blockIndex は落下後の位置)
	std::optional<BlockMoveDestination> try_move_goal_piece(
		const Vector3& playerPosition,
		const MapChipIndex& blockIndex,
		const Vector3& playerDirection,
		BlockMoveDirection moveDirection,
		r32 visualMoveDuration = 0.0f);

	/// <summary>
	/// Grip中の粘土を指定方向へ1マス伸ばし、Playerも同じ方向へ移動する
	/// </summary>
	std::optional<ClayDeformationResult> try_deform_clay(
		const Vector3& playerPosition,
		const MapChipIndex& clayIndex,
		const Vector3& playerDirection,
		BlockMoveDirection moveDirection,
		r32 visualMoveDuration = 0.0f);

private:

	/// プレイヤーの向きベクトルから、前方方向のグリッド座標オフセットを取得
	static MapChipIndex cardinal_direction(const Vector3& direction) noexcept;
	/// プレイヤーの現在マスに隣接する、向いている方向のセル(フィールド外は nullopt)
	std::optional<MapChipIndex> front_cell(const Vector3& playerPosition, const Vector3& playerDirection) const noexcept;
	static MapChipIndex relative_direction(
		const Vector3& playerDirection,
		BlockMoveDirection moveDirection) noexcept;
	std::optional<BlockMoveDestination> find_goal_piece_destination(
		const MapChipIndex& playerIndex,
		const MapChipIndex& source,
		const MapChipIndex& offset) const noexcept;

private:
	Reference<MapChipField> field_;
};
