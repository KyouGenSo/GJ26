#pragma once

struct PlayerContext;

/// <summary>
/// 複数のPlayerStateから利用する共通移動処理
/// </summary>
class PlayerMovement {
public:
	/// 入力方向へXZ平面上を移動する
	static void move_horizontal(PlayerContext& context, float moveSpeed) noexcept;
};
