#pragma once
#include <optional>
#include <string>

#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Engine/Module/World/WorldInstance/WorldInstance.h>

#include <Library/Utility/Template/Reference.h>

#include "PlayerInput.h"
#include "PlayerStateManager.h"

class FollowCamera;
class SoundPlayer;

namespace szg {
class SkinningMeshInstance;
}

/// <summary>
/// プレイヤー
/// </summary>
class Player : public szg::ISceneScript {
public:
	Player();
	explicit Player(Reference<szg::WorldInstance> worldInstance, Reference<szg::SkinningMeshInstance> meshInstance);
	virtual ~Player() = default;
	SZG_CLASS_MOVE_ONLY(Player)

public:
	/// <summary>
	/// 開放処理
	/// </summary>
	void finalize() override;

	/// <summary>
	/// World更新前処理
	/// 入力処理や、移動処理など
	/// </summary>
	void prev_update() override;

public:

	/// 入力設定を取得
	PlayerInput& get_input_mut() noexcept;
	/// 現在フレームの入力を取得
	const PlayerInputFrame& get_input_imm() const noexcept;
	/// PlayerContextを取得
	PlayerContext& get_context_mut() noexcept;
	/// PlayerContextを読み取り専用で取得
	const PlayerContext& get_context_imm() const noexcept;
	/// 操作対象のWorldInstanceを取得
	Reference<szg::WorldInstance> get_world_instance_mut() noexcept;
	/// 操作対象のWorldInstanceを読み取り専用で取得
	Reference<const szg::WorldInstance> get_world_instance_imm() const noexcept;
	/// 追従カメラを取得
	Reference<FollowCamera> get_follow_camera_mut() noexcept;
	/// stateを取得
	PlayerState get_state() const noexcept;
	/// 移動速度を取得
	float get_move_speed() const noexcept;
	/// 表示メッシュの方向追従速度を取得
	float get_mesh_turn_speed() const noexcept;
	/// 足元に支え(ブロック上面または地面)があるか
	bool is_grounded() const noexcept;
	/// プレイヤーが向いているXZ平面上のワールド方向
	const Vector3& get_direction() const noexcept;
	/// 現在掴めるブロックのインデックス
	const std::optional<MapChipIndex>& get_grip_target_index() const noexcept;
	/// 現在掴んでいるブロックのインデックス
	const std::optional<MapChipIndex>& get_gripped_block_index() const noexcept;
	/// 掴んだブロックの移動可否判定
	const std::optional<BlockMoveResult>& get_block_move_result() const noexcept;
	/// 掴んだブロックを指定方向へ移動できるか
	bool can_move_gripped_block(BlockMoveDirection direction) const noexcept;
	/// Grip中の1マス移動補間を破棄する（Undo・ステージ再読込用）
	void cancel_grip_move_interpolation() noexcept;

public:

	/// 操作対象のWorldInstanceを設定
	void set_world_instance(Reference<szg::WorldInstance> worldInstance) noexcept;
	/// 移動速度を設定
	void set_move_speed(float moveSpeed) noexcept;
	/// ジャンプ力を設定
	void set_jump_power(float jumpPower) noexcept;
	/// 落下速度を設定
	void set_fall_speed(float fallSpeed) noexcept;
	/// ブロックを掴んでいる間の移動速度を設定
	void set_grip_move_speed(float gripMoveSpeed) noexcept;
	/// 表示メッシュの方向追従速度を設定
	void set_mesh_turn_speed(float meshTurnSpeed) noexcept;
	/// プレイヤーの向きを設定(Y成分は無視、ゼロベクトルなら変更しない)
	void set_direction(const Vector3& direction) noexcept;
	/// 現在掴めるブロックを手動設定する
	void set_grip_target(const std::optional<MapChipIndex>& blockIndex) noexcept;
	/// マップチップの選択と移動可否判定を行う仲介クラスを設定する
	void set_block_movement_judge(Reference<BlockMovementJudge> judge) noexcept;
	/// Playerが操作する追従カメラを設定
	void set_follow_camera(Reference<FollowCamera> followCamera) noexcept;
	/// direction追従とアニメーション再生に使うスキニングメッシュを設定
	void set_mesh_instance(Reference<szg::SkinningMeshInstance> meshInstance);
	/// 操作に合わせて鳴らす SE(無ければ無音)
	void set_sound(Reference<SoundPlayer> sound) noexcept;

private:
	struct AnimationSetting {
		std::string fileName;
		bool isLoop{ false };
	};
	struct GripMoveInterpolation {
		Vector3 startPosition{ CVector3::ZERO };
		Vector3 targetPosition{ CVector3::ZERO };
		float elapsedSeconds{ 0.0f };
		float durationSeconds{ 0.0f };
	};

	void setup_json_asset();
	void update_animation();
	const AnimationSetting& resolve_animation_setting(PlayerState state) const noexcept;
	const AnimationSetting& resolve_grip_move_animation(BlockMoveDirection direction) const noexcept;
	void update_gripped_block_movement();
	void begin_grip_move_interpolation(
		const Vector3& targetPosition,
		float durationSeconds,
		BlockMoveDirection moveDirection);
	void update_grip_move_interpolation() noexcept;
	void update_mesh_direction(bool snap = false) noexcept;
	void update_state_sound();

private:

	Reference<szg::SkinningMeshInstance> meshInstance_;
	Reference<FollowCamera> followCamera_;
	Reference<BlockMovementJudge> blockMovementJudge_;
	Reference<SoundPlayer> sound_;
	std::string activeAnimationKey_;
	std::optional<GripMoveInterpolation> gripMoveInterpolation_;
	std::optional<BlockMoveDirection> gripMoveAnimationDirection_;
	std::string animationClipName_{ "アーマチュアアクション" };
	AnimationSetting idleAnimation_{ "playerStand.gltf", true };
	AnimationSetting moveAnimation_{ "playerWalk.gltf", true };
	AnimationSetting jumpAnimation_{ "playerJump.gltf", false };
	AnimationSetting gripAnimation_{ "playerGrab.gltf", true };
	AnimationSetting pushAnimation_{ "playerPush.gltf", false };
	AnimationSetting pullAnimation_{ "playerPull.gltf", false };
	AnimationSetting pushLeftAnimation_{ "playerPush_left.gltf", false };
	AnimationSetting pushRightAnimation_{ "playerPush_right.gltf", false };
	bool gripInputReady_{ true };
	bool gripWarnReady_{ true }; // 塞がれた面への Grip 拒否演出を押しっぱなしで繰り返さないためのゲート
	bool gripMoveInputReady_{ true };
	bool fallSoundPending_{ false }; // 押したゴール条件オブジェクトが落下中。着地音を待っている
	PlayerState previousState_{ PlayerState::Idle }; // state の切り替わりで SE を鳴らすための前フレームの state
	bool moveSoundPlaying_{ false };
	float meshTurnSpeed_{ 12.0f };

	PlayerInput playerInput_;
	PlayerContext context_;
	PlayerStateManager stateManager_;
};
