#include "PlayerMovement.h"

#include "PlayerContext.h"

//===========================================
// 入力方向へXZ平面上を移動する
//===========================================
void PlayerMovement::move_horizontal(
	PlayerContext& context,
	float moveSpeed,
	bool updateDirection) noexcept {
	if (!context.worldInstance || moveSpeed <= 0.0f || context.input.move.length() == 0.0f) {
		return;
	}

	const Vector3 moveDirection =
		context.moveRight * context.input.move.x +
		context.moveForward * context.input.move.y;
	if (updateDirection && moveDirection.length() > 0.0f) {
		context.direction = moveDirection.normalize_safe(context.direction);
	}
	const float moveDistance = moveSpeed * context.deltaSeconds;
	context.worldInstance->transform_mut().plus_translate(moveDirection * moveDistance);
}
