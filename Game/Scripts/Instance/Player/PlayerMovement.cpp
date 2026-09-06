#include "PlayerMovement.h"

#include <algorithm>
#include <cmath>

#include "PlayerContext.h"

namespace {

// 衝突箱の半サイズ。高さは1セル(原点±0.5)、XZはセルより細くして壁沿いに滑れるようにする
constexpr Vector3 kHalfExtent{ 0.3f, 0.5f, 0.3f };
// 境界にぴったり接した状態を「重なり無し」側に落とす余白(float誤差より十分大きい)
constexpr float kSkin = 1e-3f;
// 1回で動かす量の上限。deltaSecondsが跳ねてもセルを飛び越えない
constexpr float kMaxStep = 0.5f;
// 接地中に毎フレーム下を探る量。重力だけだと1フレームの下降がkSkinを下回って接地が途切れる
constexpr float kGroundProbe = 0.02f;

} // namespace

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
	const Vector3 delta = moveDirection * (moveSpeed * context.deltaSeconds);
	move_axis(context, 0, delta.x);
	move_axis(context, 2, delta.z);
}

//===========================================
// 重力を積分してYを衝突付きで動かし、接地を更新する
//===========================================
void PlayerMovement::apply_gravity(PlayerContext& context) noexcept {
	if (!context.worldInstance) {
		return;
	}

	context.verticalVelocity -= context.fallSpeed * context.deltaSeconds;
	const float velocity = context.verticalVelocity;
	float deltaY = velocity * context.deltaSeconds;
	// 接地中は必ず少し下を探って支えの有無を確かめる(上昇中は探らない)
	if (context.isGrounded && velocity <= 0.0f) {
		deltaY = std::min(deltaY, -kGroundProbe);
	}
	const bool hitBlock = move_axis(context, 1, deltaY);

	// 地面は足元y=-0.5(原点y=0)の平面
	Vector3& position = context.worldInstance->transform_mut().get_translate();
	const bool onGround = position.y <= 0.0f;
	if (onGround) {
		position.y = 0.0f;
	}

	context.isGrounded = velocity <= 0.0f && (hitBlock || onGround);
	if (hitBlock || onGround) {
		context.verticalVelocity = 0.0f;
	}
}

//===========================================
// 1軸だけ動かし、固体に入ったら進入した面へ押し戻す
//===========================================
bool PlayerMovement::move_axis(PlayerContext& context, size_t axis, float delta) noexcept {
	Vector3& position = context.worldInstance->transform_mut().get_translate();
	const float sign = delta < 0.0f ? -1.0f : 1.0f;
	const Vector3 skin{ kSkin, kSkin, kSkin };
	while (delta != 0.0f) {
		const float step = std::clamp(delta, -kMaxStep, kMaxStep);
		delta -= step;
		position[axis] += step;
		if (!context.judge ||
			!context.judge->overlaps_solid(position - kHalfExtent + skin, position + kHalfExtent - skin)) {
			continue;
		}
		// 進行方向の先頭面が入ったセルの、手前の面に接する位置へ戻す
		const float lead = position[axis] + sign * kHalfExtent[axis];
		const float cell = std::floor(lead - sign * kSkin + 0.5f);
		position[axis] = cell - sign * (0.5f + kHalfExtent[axis]);
		return true;
	}
	return false;
}
