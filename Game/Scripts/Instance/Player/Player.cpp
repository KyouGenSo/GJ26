#include "Player.h"

#include <Engine/Runtime/Clock/WorldClock.h>

Player::Player(Reference<szg::WorldInstance> worldInstance_) noexcept {
	set_world_instance(worldInstance_);
}

//================================
// 開放処理
//================================
void Player::finalize() {
	stateManager_.reset(context_);
	context_ = {};
	meshInstance_.reset();
}

//================================
// world更新前処理
//================================
void Player::prev_update() {
	context_.input = playerInput_.update();
	context_.deltaSeconds = szg::WorldClock::DeltaSeconds();
	stateManager_.update(context_);
}

//================================
// world更新後処理
//================================
void Player::post_update() {
	if (!context_.worldInstance) {
		context_.isGrounded = false;
		return;
	}

	update_grounded(context_.worldInstance->world_position().y);
}

//================================
// 操作対象のWorldInstanceを設定
//================================
void Player::set_world_instance(Reference<szg::WorldInstance> worldInstance_) noexcept {
	context_.worldInstance = worldInstance_;

	if (!context_.worldInstance) {
		context_.isGrounded = false;
		return;
	}

	update_grounded(context_.worldInstance->transform_imm().get_translate().y);
}

//================================
// 入力設定の取得
//================================
PlayerInput& Player::get_input_mut() noexcept {
	return playerInput_;
}

//================================
// 現在フレームの入力を取得
//================================
const PlayerInputFrame& Player::get_input_imm() const noexcept {
	return context_.input;
}

//================================
// PlayerContextの取得
//================================
PlayerContext& Player::get_context_mut() noexcept {
	return context_;
}

//================================
// PlayerContextの読み取り専用取得
//================================
const PlayerContext& Player::get_context_imm() const noexcept {
	return context_;
}

//================================
// 状態の取得
//================================
PlayerState Player::get_state() const noexcept {
	return stateManager_.get_current_state();
}

//================================
// 移動速度の設定
//================================
void Player::set_move_speed(float moveSpeed) noexcept {
	context_.moveSpeed = moveSpeed < 0.0f ? 0.0f : moveSpeed;
}

void Player::set_jump_power(float jumpPower) noexcept {
	context_.jumpPower = jumpPower < 0.0f ? 0.0f : jumpPower;
}

void Player::set_fall_speed(float fallSpeed) noexcept {
	context_.fallSpeed = fallSpeed < 0.0f ? 0.0f : fallSpeed;
}

void Player::set_grip_move_speed(float gripMoveSpeed) noexcept {
	context_.gripMoveSpeed = gripMoveSpeed < 0.0f ? 0.0f : gripMoveSpeed;
}

void Player::set_can_grip(bool canGrip) noexcept {
	context_.canGrip = canGrip;
}

void Player::set_mesh_instance(Reference<szg::SkinningMeshInstance> meshInstance) {
	meshInstance_ = meshInstance;
}

//================================
// 移動速度の取得
//================================
float Player::get_move_speed() const noexcept {
	return context_.moveSpeed;
}

//================================
// 接地状態の更新
//================================
void Player::update_grounded(float positionY) noexcept {
	context_.isGrounded = positionY == 0.0f;
}

//================================
// 接地状態の取得
//================================
bool Player::is_grounded() const noexcept {
	return context_.isGrounded;
}
