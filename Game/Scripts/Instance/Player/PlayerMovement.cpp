#include "PlayerMovement.h"

#include "PlayerContext.h"

void PlayerMovement::move_horizontal(PlayerContext& context, float moveSpeed) noexcept {
	if (!context.worldInstance || moveSpeed <= 0.0f || context.input.move.length() == 0.0f) {
		return;
	}

	const Vector3 moveDirection =
		context.moveRight * context.input.move.x +
		context.moveForward * context.input.move.y;
	const float moveDistance = moveSpeed * context.deltaSeconds;
	context.worldInstance->transform_mut().plus_translate(moveDirection * moveDistance);
}
