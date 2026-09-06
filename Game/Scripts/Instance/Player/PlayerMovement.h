#pragma once

#include <cstddef>

struct PlayerContext;

/// <summary>
/// 複数のPlayerStateから利用する共通移動処理
/// </summary>
class PlayerMovement {
public:
	/// 入力方向へXZ平面上を移動する(ブロック・ステージ端で止まる)
	static void move_horizontal(
		PlayerContext& context,
		float moveSpeed,
		bool updateDirection = true) noexcept;

	/// 重力を積分してYを衝突付きで動かし、isGroundedを更新する(毎フレーム、全State共通)
	static void apply_gravity(PlayerContext& context) noexcept;

private:
	/// 1軸だけdelta動かし、固体に入ったら進入した面へ押し戻す。押し戻したらtrue
	static bool move_axis(PlayerContext& context, size_t axis, float delta) noexcept;
};
