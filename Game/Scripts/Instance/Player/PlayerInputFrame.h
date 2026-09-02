#pragma once

#include <Library/Math/Vector2.h>

enum class PlayerInputAction {
	Jump,
	Push,
	Pull,
};

/// <summary>
/// 1フレーム分のプレイヤー入力
/// </summary>
struct PlayerInputFrame {
	Vector2 move;
	bool jumpTriggered{ false };
	bool pushPressed{ false };
	bool pullPressed{ false };
};
