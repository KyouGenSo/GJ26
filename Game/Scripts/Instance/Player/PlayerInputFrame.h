#pragma once

#include <Library/Math/Vector2.h>

enum class PlayerInputAction {
	Jump,
	Grip,
};

/// <summary>
/// 1フレーム分のプレイヤー入力
/// </summary>
struct PlayerInputFrame {
	Vector2 move;
	/// Pad右スティックなどの継続的な旋回入力
	Vector2 cameraRotationInput;
	/// マウスなどの現在フレームに発生した旋回量
	Vector2 cameraRotationDelta;
	bool jumpTriggered{ false };
	/// ブロックを掴む入力が押されている
	bool gripPressed{ false };
};
