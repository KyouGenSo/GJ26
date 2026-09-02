#include "Player.h"

#include <Engine/Runtime/Clock/WorldClock.h>

Player::Player(Reference<szg::WorldInstance> worldInstance_) noexcept {
	set_world_instance(worldInstance_);
}

//================================
// 開放処理
//================================
void Player::finalize() {
	worldInstance.reset();
	inputFrame = {};
	state = PlayerState::Idle;
	isGrounded = false;
}

//================================
// world更新前処理
//================================
void Player::prev_update() {
	inputFrame = playerInput.update();
	update_movement();
}

//================================
// world更新後処理
//================================
void Player::post_update() {
	if (!worldInstance) {
		isGrounded = false;
		return;
	}

	update_grounded(worldInstance->world_position().y);
}

//================================
// 操作対象のWorldInstanceを設定
//================================
void Player::set_world_instance(Reference<szg::WorldInstance> worldInstance_) noexcept {
	worldInstance = worldInstance_;

	if (!worldInstance) {
		isGrounded = false;
		return;
	}

	update_grounded(worldInstance->transform_imm().get_translate().y);
}

//================================
// 入力設定の取得
//================================
PlayerInput& Player::input_mut() noexcept {
	return playerInput;
}

//================================
// 現在フレームの入力を取得
//================================
const PlayerInputFrame& Player::input_imm() const noexcept {
	return inputFrame;
}

//================================
// 状態の設定
//================================
void Player::set_state(PlayerState state_) {
	state = state_;
}

//================================
// 状態の取得
//================================
PlayerState Player::state_imm() const noexcept {
	return state;
}

//================================
// 移動速度の設定
//================================
void Player::set_move_speed(float moveSpeed_) noexcept {
	moveSpeed = moveSpeed_ < 0.0f ? 0.0f : moveSpeed_;
}

//================================
// 移動速度の取得
//================================
float Player::move_speed() const noexcept {
	return moveSpeed;
}

//================================
// 移動処理
//================================
void Player::update_movement() {
	if (inputFrame.move.length() == 0.0f) {
		set_state(PlayerState::Idle);
		return;
	}

	set_state(PlayerState::Move);
	if (!worldInstance) {
		return;
	}

	const Vector3 moveDirection{
		inputFrame.move.x,
		0.0f,
		inputFrame.move.y,
	};
	const float moveDistance = moveSpeed * szg::WorldClock::DeltaSeconds();
	worldInstance->transform_mut().plus_translate(moveDirection * moveDistance);
}

//================================
// 接地状態の更新
//================================
void Player::update_grounded(float positionY) noexcept {
	isGrounded = positionY == 0.0f;
}

//================================
// 接地状態の取得
//================================
bool Player::is_grounded() const noexcept {
	return isGrounded;
}
