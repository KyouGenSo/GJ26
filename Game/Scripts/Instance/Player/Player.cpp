#include "Player.h"

#include <Engine/Runtime/Clock/WorldClock.h>

#include "Scripts/Instance/FollowCamera/FollowCamera.h"

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
	followCamera_.reset();
	blockMovementJudge_.reset();
}

//================================
// world更新前処理
//================================
void Player::prev_update() {
	context_.input = playerInput_.update();
	context_.deltaSeconds = szg::WorldClock::DeltaSeconds();
	if (followCamera_) {
		followCamera_->add_rotation_input(context_.input.cameraRotationInput);
		followCamera_->add_rotation_delta(context_.input.cameraRotationDelta);
		context_.moveForward = followCamera_->get_horizontal_forward();
		context_.moveRight = followCamera_->get_horizontal_right();
	}
	else {
		context_.moveForward = { 0.0f, 0.0f, 1.0f };
		context_.moveRight = { 1.0f, 0.0f, 0.0f };
	}

	if (blockMovementJudge_ && context_.worldInstance && !context_.grippedBlockIndex) {
		context_.gripTargetIndex = blockMovementJudge_->find_grip_target(
			context_.worldInstance->world_position(), context_.direction);
	}
	stateManager_.update(context_);

	if (blockMovementJudge_ && context_.grippedBlockIndex) {
		context_.blockMoveResult = blockMovementJudge_->judge(
			*context_.grippedBlockIndex, context_.direction);
	}
	else {
		context_.blockMoveResult.reset();
	}
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
// 操作対象のWorldInstanceの取得
//================================
Reference<szg::WorldInstance> Player::get_world_instance_mut() noexcept {
	return context_.worldInstance;
}

//================================
// 操作対象のWorldInstanceの読み取り専用取得
//================================
Reference<const szg::WorldInstance> Player::get_world_instance_imm() const noexcept {
	return context_.worldInstance;
}

//================================
// 追従カメラの取得
//================================
Reference<FollowCamera> Player::get_follow_camera_mut() noexcept {
	return followCamera_;
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

void Player::set_direction(const Vector3& direction) noexcept {
	const Vector3 horizontal{ direction.x, 0.0f, direction.z };
	if (horizontal.length() > 0.0f) {
		context_.direction = horizontal.normalize_safe(context_.direction);
	}
}

void Player::set_grip_target(const std::optional<MapChipIndex>& blockIndex) noexcept {
	if (!context_.grippedBlockIndex) {
		context_.gripTargetIndex = blockIndex;
	}
}

void Player::set_block_movement_judge(Reference<const BlockMovementJudge> judge) noexcept {
	blockMovementJudge_ = judge;
}

void Player::set_follow_camera(Reference<FollowCamera> followCamera) noexcept {
	followCamera_ = followCamera;
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

const Vector3& Player::get_direction() const noexcept {
	return context_.direction;
}

const std::optional<MapChipIndex>& Player::get_grip_target_index() const noexcept {
	return context_.gripTargetIndex;
}

const std::optional<MapChipIndex>& Player::get_gripped_block_index() const noexcept {
	return context_.grippedBlockIndex;
}

const std::optional<BlockMoveResult>& Player::get_block_move_result() const noexcept {
	return context_.blockMoveResult;
}

bool Player::can_move_gripped_block(BlockMoveDirection direction) const noexcept {
	return context_.blockMoveResult && context_.blockMoveResult->can_move(direction);
}
