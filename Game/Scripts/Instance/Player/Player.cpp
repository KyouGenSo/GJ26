#include "Player.h"

#include <algorithm>
#include <cmath>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/Animation/NodeAnimation/NodeAnimationLibrary.h>
#include <Engine/Assets/Animation/NodeAnimation/NodeAnimationPlayer.h>
#include <Engine/Assets/Json/JsonAsset.h>
#include <Engine/Module/World/Mesh/SkinningMeshInstance.h>
#include <Engine/Runtime/Clock/WorldClock.h>

#include "PlayerMovement.h"
#include "Scripts/Instance/FollowCamera/FollowCamera.h"
#include "Scripts/Manager/SoundPlayer.h"

namespace {

const float kGripMoveTriggerThreshold = 0.5f;
const float kGripMoveResetThreshold = 0.25f;

/// プレイヤーの向きベクトルを、X軸またはZ軸のいずれかにスナップする処理
Vector3 SnapToCardinalDirection(const Vector3& direction) noexcept {
	if (std::abs(direction.x) > std::abs(direction.z)) {
		return { direction.x < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f };
	}
	return { 0.0f, 0.0f, direction.z < 0.0f ? -1.0f : 1.0f };
}

/// プレイヤーの向きベクトルと入力方向から、ブロックを押す方向を決定する
std::optional<BlockMoveDirection> ResolveBlockMoveDirection(const PlayerContext& context) noexcept {
	const Vector3 cameraRelativeDirection =
		context.moveRight * context.input.move.x +
		context.moveForward * context.input.move.y;
	if (cameraRelativeDirection.length() == 0.0f) {
		return std::nullopt;
	}

	const Vector3 moveDirection = SnapToCardinalDirection(cameraRelativeDirection);
	const Vector3 playerForward = SnapToCardinalDirection(context.direction);
	const Vector3 playerRight{ playerForward.z, 0.0f, -playerForward.x };
	const float forwardAlignment =
		moveDirection.x * playerForward.x + moveDirection.z * playerForward.z;
	if (std::abs(forwardAlignment) > 0.5f) {
		return forwardAlignment > 0.0f
			? BlockMoveDirection::Forward
			: BlockMoveDirection::Backward;
	}

	const float rightAlignment =
		moveDirection.x * playerRight.x + moveDirection.z * playerRight.z;
	return rightAlignment > 0.0f
		? BlockMoveDirection::Right
		: BlockMoveDirection::Left;
}

bool IsGripMoveDirectionHeld(
	const PlayerContext& context,
	BlockMoveDirection direction) noexcept {
	if (context.input.move.length() < kGripMoveTriggerThreshold) {
		return false;
	}
	const std::optional<BlockMoveDirection> heldDirection = ResolveBlockMoveDirection(context);
	return heldDirection && *heldDirection == direction;
}

} // namespace

Player::Player() {
	setup_json_asset();
}

Player::Player(
	Reference<szg::WorldInstance> worldInstance_,
 Reference<szg::SkinningMeshInstance> meshInstance) : Player() {
	set_world_instance(worldInstance_);
	set_mesh_instance(meshInstance);
}

//================================
// 開放処理
//================================
void Player::finalize() {
	stateManager_.reset(context_);
	context_ = {};
	context_.worldInstance.reset();
	meshInstance_.reset();
	followCamera_.reset();
	blockMovementJudge_.reset();
	sound_.reset();
	activeAnimationKey_.clear();
	gripMoveInterpolation_.reset();
	gripMoveAnimationDirection_.reset();
	gripInputReady_ = true;
	gripWarnReady_ = true;
	gripMoveInputReady_ = true;
	previousState_ = PlayerState::Idle;
	moveSoundPlaying_ = false;
	inputEnabled_ = true;
	gravityEnabled_ = true;
	clearPresentationActive_ = false;
	clearAnimationFinished_ = false;
}

//================================
// world更新前処理
//================================
void Player::prev_update() {
	const PlayerInputFrame polledInput = playerInput_.update();
	context_.input = inputEnabled_ ? polledInput : PlayerInputFrame{};
	if (!context_.input.gripPressed) {
		gripInputReady_ = true;
		gripWarnReady_ = true;
	}
	if (!gripInputReady_) {
		context_.input.gripPressed = false;
	}
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

	if (!gripMoveInterpolation_ && blockMovementJudge_ && context_.worldInstance && !context_.grippedBlockIndex) {
		context_.gripTargetIndex = blockMovementJudge_->find_grip_target(
			context_.worldInstance->world_position(), context_.direction);
		// 掴めない(前に無い / 落下中 / 塞がれた面)のに Grip を押したら音で知らせ、塞がれた面ならその面の cross も出す(押しっぱなしでは 1 回だけ)
		if (gripWarnReady_ && context_.input.gripPressed &&
			(!context_.gripTargetIndex || !context_.isGrounded)) {
			gripWarnReady_ = false;
			if (!context_.gripTargetIndex) {
				blockMovementJudge_->warn_blocked_grip(context_.worldInstance->world_position(), context_.direction);
			}
			if (sound_) {
				sound_->restart("cantGrab.wav");
			}
		}
	}
	bool gravityApplied = false;
	const bool wasGripInterpolating = gripMoveInterpolation_.has_value();
	if (wasGripInterpolating) {
		update_grip_move_interpolation();
	}
	if (!gripMoveInterpolation_) {
		// 次のブロック操作より先に移動先の接地を更新する。
		// 足場がなければGripStateが掴みを解除し、連続伸長せず落下する。
		if (gravityEnabled_ &&
			(wasGripInterpolating || stateManager_.get_current_state() == PlayerState::Grip)) {
			PlayerMovement::apply_gravity(context_);
			gravityApplied = true;
		}
		stateManager_.update(context_);
		update_gripped_block_movement();
	}
	// 落下中のゴール条件オブジェクトの補間が終わった(着地した)フレームで 1 回だけ鳴らす
	if (fallSoundPending_ && blockMovementJudge_ && !blockMovementJudge_->is_visual_interpolating()) {
		fallSoundPending_ = false;
		if (sound_) {
			sound_->restart("objectFall.wav");
		}
	}
	update_state_sound();
	update_animation();
	update_clear_animation_sequence();
	// Gripのグリッド移動中は、補間位置が重力やブロック衝突で上書きされないようにする。
	if (gravityEnabled_ && !gripMoveInterpolation_ && !gravityApplied) {
		PlayerMovement::apply_gravity(context_);
	}
	update_mesh_direction();

	if (!gripMoveInterpolation_ && blockMovementJudge_ && context_.worldInstance && context_.grippedBlockIndex &&
		blockMovementJudge_->is_goal_piece(*context_.grippedBlockIndex)) {
		context_.blockMoveResult = blockMovementJudge_->judge(
			context_.worldInstance->world_position(),
			*context_.grippedBlockIndex,
			context_.direction);
	}
	else {
		context_.blockMoveResult.reset();
	}
}

//================================
// 操作対象のWorldInstanceを設定
//================================
void Player::set_world_instance(Reference<szg::WorldInstance> worldInstance_) noexcept {
	cancel_grip_move_interpolation();
	context_.worldInstance = worldInstance_;
	context_.isGrounded = false;
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

void Player::set_mesh_turn_speed(float meshTurnSpeed) noexcept {
	meshTurnSpeed_ = std::max(meshTurnSpeed, 0.0f);
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

void Player::set_block_movement_judge(Reference<BlockMovementJudge> judge) noexcept {
	blockMovementJudge_ = judge;
	context_.judge = judge;
}

void Player::set_follow_camera(Reference<FollowCamera> followCamera) noexcept {
	followCamera_ = followCamera;
}

void Player::set_sound(Reference<SoundPlayer> sound) noexcept {
	sound_ = sound;
}

void Player::set_mesh_instance(Reference<szg::SkinningMeshInstance> meshInstance) {
	meshInstance_ = meshInstance;
	activeAnimationKey_.clear();
	update_mesh_direction(true);
	update_animation();
}

void Player::set_input_enabled(bool enabled) noexcept {
	if (inputEnabled_ == enabled) {
		return;
	}

	inputEnabled_ = enabled;
	if (inputEnabled_) {
		return;
	}

	// クリア演出開始フレームの入力を残さず、Gripも安全に終了させる。
	context_.input = {};
	stateManager_.reset(context_);
	context_.gripTargetIndex.reset();
	context_.blockMoveResult.reset();
	gripMoveInputReady_ = true;
}

//================================
// 移動速度の取得
//================================
float Player::get_move_speed() const noexcept {
	return context_.moveSpeed;
}

float Player::get_mesh_turn_speed() const noexcept {
	return meshTurnSpeed_;
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

bool Player::is_input_enabled() const noexcept {
	return inputEnabled_;
}

void Player::start_clear_presentation() {
	if (clearPresentationActive_) {
		return;
	}

	clearPresentationActive_ = true;
	clearAnimationFinished_ = false;
	gravityEnabled_ = false;
	context_.verticalVelocity = 0.0f;
	activeAnimationKey_.clear();
	update_animation();
}

void Player::stop_clear_presentation() {
	if (!clearPresentationActive_) {
		gravityEnabled_ = true;
		return;
	}

	clearPresentationActive_ = false;
	clearAnimationFinished_ = false;
	gravityEnabled_ = true;
	context_.verticalVelocity = 0.0f;
	activeAnimationKey_.clear();
	update_animation();
}

void Player::cancel_grip_move_interpolation() noexcept {
	gripMoveInterpolation_.reset();
	gripMoveAnimationDirection_.reset();
	fallSoundPending_ = false;
}

//================================
// Player用パラメータの読み込み
//================================
void Player::setup_json_asset() {
	szg::JsonAsset parameter{ "[[game]]/PlayerInit.param" };
	const nlohmann::json& json = parameter.cget();

	const auto readString = [&json](const char* name, const std::string& fallback) {
		return json.value(name, nlohmann::json::object()).value("value", fallback);
	};
	const auto readFloat = [&json](const char* name, float fallback) {
		return json.value(name, nlohmann::json::object()).value("value", fallback);
	};

	animationClipName_ = readString("AnimationClipName", animationClipName_);
	idleAnimation_.fileName = readString("IdleAnimationFile", idleAnimation_.fileName);
	moveAnimation_.fileName = readString("MoveAnimationFile", moveAnimation_.fileName);
	jumpAnimation_.fileName = readString("JumpAnimationFile", jumpAnimation_.fileName);
	gripAnimation_.fileName = readString("GripAnimationFile", gripAnimation_.fileName);
	pushAnimation_.fileName = readString("PushAnimationFile", pushAnimation_.fileName);
	pullAnimation_.fileName = readString("PullAnimationFile", pullAnimation_.fileName);
	pushLeftAnimation_.fileName = readString("PushLeftAnimationFile", pushLeftAnimation_.fileName);
	pushRightAnimation_.fileName = readString("PushRightAnimationFile", pushRightAnimation_.fileName);
	clearAnimation_.fileName = readString("ClearAnimationFile", clearAnimation_.fileName);
	clearStandAnimation_.fileName = readString(
		"ClearStandAnimationFile", clearStandAnimation_.fileName);
	set_move_speed(readFloat("移動スピード", context_.moveSpeed));
	set_jump_power(readFloat("ジャンプ力", context_.jumpPower));
	set_fall_speed(readFloat("FallSpeed", context_.fallSpeed));
	set_grip_move_speed(readFloat("GripMoveSpeed", context_.gripMoveSpeed));
	szgInformation(
		"Player: movement parameter loaded. MoveSpeed-{}, JumpPower-{}, FallSpeed-{}.",
		context_.moveSpeed,
		context_.jumpPower,
		context_.fallSpeed);
}

//================================
// PlayerStateに対応するアニメーションへ切り替える
//================================
void Player::update_animation() {
	if (!meshInstance_) {
		return;
	}
	
	const PlayerState state = stateManager_.get_current_state();
	const AnimationSetting& setting = clearPresentationActive_
		? (clearAnimationFinished_ ? clearStandAnimation_ : clearAnimation_)
		: gripMoveInterpolation_ && gripMoveAnimationDirection_
			? resolve_grip_move_animation(*gripMoveAnimationDirection_)
			: resolve_animation_setting(state);
	const std::string animationKey = setting.fileName + '-' + animationClipName_;
	if (activeAnimationKey_ == animationKey) {
		return;
	}

	// アニメーションが登録されていない場合は警告を出して終了
	if (!szg::NodeAnimationLibrary::IsRegistered(animationKey)) {
		szgWarning("Player: animation is not registered. Name-'{}'.", animationKey);
		activeAnimationKey_ = animationKey;
		return;
	}

	// アニメーションを切り替える
	meshInstance_->reset_animation(
		setting.fileName,
		animationClipName_,
		setting.isLoop);
	if (szg::NodeAnimationPlayer* animation = meshInstance_->get_animation()) {
		animation->restart();
	}
	activeAnimationKey_ = animationKey;
}

//================================
// クリア動作の終了後、クリア待機アニメーションへ切り替える
//================================
void Player::update_clear_animation_sequence() {
	if (!clearPresentationActive_ || clearAnimationFinished_ || !meshInstance_) {
		return;
	}

	const szg::NodeAnimationPlayer* animation = meshInstance_->get_animation();
	if (!animation || !animation->is_end()) {
		return;
	}

	clearAnimationFinished_ = true;
	activeAnimationKey_.clear();
	update_animation();
}

//================================
// Grip中の移動方向に対応するアニメーション設定を取得
//================================
const Player::AnimationSetting& Player::resolve_grip_move_animation(BlockMoveDirection direction) const noexcept {
	switch (direction) {
	case BlockMoveDirection::Backward:
		return pullAnimation_;
	case BlockMoveDirection::Left:
		return pushLeftAnimation_;
	case BlockMoveDirection::Right:
		return pushRightAnimation_;
	case BlockMoveDirection::Forward:
	default:
		return pushAnimation_;
	}
}

//================================
// PlayerStateに対応するアニメーション設定を取得
//================================
const Player::AnimationSetting& Player::resolve_animation_setting(PlayerState state) const noexcept {
	switch (state) {
	case PlayerState::Move:
		return moveAnimation_;
	case PlayerState::Jump:
		return jumpAnimation_;
	case PlayerState::Grip:
		return gripAnimation_;
	case PlayerState::Idle:
	default:
		return idleAnimation_;
	}
}

//================================
// Grip中の入力でPlayerと対象ブロックを1マス操作する
//================================
void Player::update_gripped_block_movement() {

	// Grip中でない、またはブロック移動判定が設定されていない場合は何もしない
	if (stateManager_.get_current_state() != PlayerState::Grip ||
		!context_.isGrounded || !blockMovementJudge_ || !context_.worldInstance || !context_.grippedBlockIndex) {
		gripMoveInputReady_ = true;
		return;
	}

	// Grip中の入力が小さい場合は、次の大きな入力を待つ
	const float inputLength = context_.input.move.length();
	if (inputLength <= kGripMoveResetThreshold) {
		gripMoveInputReady_ = true;
		return;
	}
	if (!gripMoveInputReady_ || inputLength < kGripMoveTriggerThreshold) {
		return;
	}

	// Grip中の入力が大きい場合は、ブロック移動判定を行う
	gripMoveInputReady_ = false;
	const std::optional<BlockMoveDirection> moveDirection = ResolveBlockMoveDirection(context_);
	if (!moveDirection) {
		return;
	}

	// 補間完了・接地更新後の座標で次のマスを判定する。
	context_.worldInstance->update_affine();

	// 粘土の伸縮判定を行う
	const float moveDuration = context_.gripMoveSpeed > 0.0f
		? 1.0f / context_.gripMoveSpeed
		: 0.0f;
	const std::optional<ClayDeformationResult> deformation = blockMovementJudge_->try_deform_clay(
		context_.worldInstance->world_position(),
		*context_.grippedBlockIndex,
		context_.direction,
		*moveDirection,
		moveDuration);
	if (deformation) {
		begin_grip_move_interpolation(MapChipField::to_world(
			deformation->playerIndex.x,
			deformation->playerIndex.y,
			deformation->playerIndex.z), moveDuration, *moveDirection);

		// 粘土を伸ばした場合は、プレイヤーと粘土の両方が移動するので、グリップ状態を解除する
		if (deformation->type == ClayDeformationType::Connect) {
			gripInputReady_ = false;
			context_.input.gripPressed = false;
			stateManager_.release_grip(context_);
			if (sound_) {
				sound_->restart("clayConnect.wav");
			}
		}
		else {
			context_.grippedBlockIndex = deformation->clayIndex;
			if (sound_) {
				sound_->restart("stretch.wav");
			}
		}

		// 粘土の伸縮操作をログに出力する
		const char* operation = deformation->type == ClayDeformationType::Stretch
			? "stretched"
			: "connected";
		szgInformation(
			"Player: {} Clay. player=({}, {}, {}), clay=({}, {}, {})",
			operation,
			deformation->playerIndex.x,
			deformation->playerIndex.y,
			deformation->playerIndex.z,
			deformation->clayIndex.x,
			deformation->clayIndex.y,
			deformation->clayIndex.z);
		return;
	}

	// ゴール条件オブジェクトの移動判定を行う
	const std::optional<BlockMoveDestination> move = blockMovementJudge_->try_move_goal_piece(
		context_.worldInstance->world_position(),
		*context_.grippedBlockIndex,
		context_.direction,
		*moveDirection,
		moveDuration);
	// 移動できない場合はブロックを振動させて知らせる
	if (!move) {
		blockMovementJudge_->warn_block_stuck(*context_.grippedBlockIndex, context_.direction, *moveDirection);
		if (sound_) {
			sound_->restart("cantMove.wav");
		}
		return;
	}

	// ゴール条件オブジェクトを移動する
	if (sound_) {
		sound_->restart("objectMove.wav");
	}
	begin_grip_move_interpolation(MapChipField::to_world(
		move->playerIndex.x,
		move->playerIndex.y,
		move->playerIndex.z), moveDuration, *moveDirection);

	// 落下したブロックはプレイヤーより下の段になるので、グリップ状態を解除する
	if (move->blockIndex.y < context_.grippedBlockIndex->y) {
		gripInputReady_ = false;
		context_.input.gripPressed = false;
		stateManager_.release_grip(context_);
		fallSoundPending_ = true;
	}
	else {
		context_.grippedBlockIndex = move->blockIndex;
	}
	szgInformation(
		"Player: moved grabbed GoalPiece. player=({}, {}, {}), block=({}, {}, {})",
		move->playerIndex.x,
		move->playerIndex.y,
		move->playerIndex.z,
		move->blockIndex.x,
		move->blockIndex.y,
		move->blockIndex.z);
}

//================================
// Grip中の1マス移動補間を開始する
//================================
void Player::begin_grip_move_interpolation(
	const Vector3& targetPosition,
	float durationSeconds,
	BlockMoveDirection moveDirection) {
	if (!context_.worldInstance) {
		return;
	}

	const Vector3 startPosition = context_.worldInstance->transform_imm().get_translate();
	if (durationSeconds <= 0.0f || Vector3::Length(startPosition, targetPosition) <= 0.0001f) {
		context_.worldInstance->transform_mut().set_translate(targetPosition);
		gripMoveInterpolation_.reset();
		gripMoveAnimationDirection_.reset();
		gripMoveInputReady_ = IsGripMoveDirectionHeld(context_, moveDirection);
		return;
	}

	gripMoveInterpolation_ = GripMoveInterpolation{
		.startPosition = startPosition,
		.targetPosition = targetPosition,
		.elapsedSeconds = 0.0f,
		.durationSeconds = durationSeconds,
	};
	gripMoveAnimationDirection_ = moveDirection;
}

//================================
// Grip中のPlayer移動をブロック表示と同じSmoothStepで補間する
//================================
void Player::update_grip_move_interpolation() noexcept {
	if (!gripMoveInterpolation_ || !context_.worldInstance) {
		gripMoveInterpolation_.reset();
		gripMoveAnimationDirection_.reset();
		return;
	}

	GripMoveInterpolation& interpolation = *gripMoveInterpolation_;
	interpolation.elapsedSeconds += std::max(context_.deltaSeconds, 0.0f);
	const float duration = std::max(interpolation.durationSeconds, 0.001f);
	const float t = std::clamp(interpolation.elapsedSeconds / duration, 0.0f, 1.0f);
	const float eased = t * t * (3.0f - 2.0f * t);
	context_.worldInstance->transform_mut().set_translate(Vector3::Lerp(
		interpolation.startPosition,
		interpolation.targetPosition,
		eased));

	if (t >= 1.0f) {
		const std::optional<BlockMoveDirection> completedDirection = gripMoveAnimationDirection_;
		gripMoveInterpolation_.reset();
		gripMoveAnimationDirection_.reset();
		gripMoveInputReady_ = completedDirection &&
			IsGripMoveDirectionHeld(context_, *completedDirection);
	}
}

//================================
// directionに表示メッシュの前方(+Z)を合わせる
//================================
void Player::update_mesh_direction(bool snap) noexcept {
	if (!meshInstance_) {
		return;
	}

	const Vector3 horizontalDirection{
		context_.direction.x,
		0.0f,
		context_.direction.z,
	};
	if (horizontalDirection.length() == 0.0f) {
		return;
	}

	const Quaternion targetRotation =
		Quaternion::LookForward(horizontalDirection.normalize_safe(CVector3::BASIS_Z));
	auto& transform = meshInstance_->transform_mut();
	if (snap) {
		transform.set_quaternion(targetRotation);
		return;
	}

	const float interpolation = std::clamp(
		1.0f - std::exp(-meshTurnSpeed_ * context_.deltaSeconds),
		0.0f,
		1.0f);
	transform.set_quaternion(Quaternion::Slerp(
		transform.get_quaternion(),
		targetRotation,
		interpolation).normalize());
}

//================================
// state の切り替わりと接地中の移動に合わせて SE を鳴らす
//================================
void Player::update_state_sound() {
	const PlayerState state = stateManager_.get_current_state();
	if (state != previousState_) {
		// Jump は接地中にトリガーした時だけ入るので、空中で押しても鳴らない
		if (state == PlayerState::Jump && sound_) {
			sound_->restart("jump.wav");
		}
		if (state == PlayerState::Grip) {
			// 掴んだまま Undo で外れた直後に cantGrab が鳴らないよう、この押下は警告済み扱いにする
			gripWarnReady_ = false;
			if (sound_) {
				sound_->restart("grab.wav");
			}
		}
		previousState_ = state;
	}

	// 移動音は接地中の Move 状態だけループさせる(空中の横移動では鳴らさない)
	const bool moving = state == PlayerState::Move && context_.isGrounded;
	if (moving == moveSoundPlaying_) {
		return;
	}
	moveSoundPlaying_ = moving;
	if (!sound_) {
		return;
	}
	if (moving) {
		sound_->play("move.wav");
	}
	else {
		sound_->stop("move.wav");
	}
}
