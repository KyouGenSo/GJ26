#pragma once
#include <optional>

#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Engine/Module/World/WorldInstance/WorldInstance.h>
#include <Engine/Module/World/Mesh/SkinningMeshInstance.h>

#include <Library/Utility/Template/Reference.h>

#include "PlayerInput.h"
#include "PlayerStateManager.h"

class FollowCamera;

/// <summary>
/// プレイヤー
/// </summary>
class Player : public szg::ISceneScript {
public:
	Player() = default;
	explicit Player(Reference<szg::WorldInstance> worldInstance) noexcept;
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

	/// <summary>
	/// World更新後処理
	/// カメラの更新や、UIの更新など
	///　</summary>
	void post_update() override;

	/// <summary>
	/// プレイヤーのY座標から接地状態を更新
	/// </summary>
	void update_grounded(float positionY) noexcept;

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
	/// 接地しているか
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
	/// プレイヤーの向きを設定(Y成分は無視、ゼロベクトルなら変更しない)
	void set_direction(const Vector3& direction) noexcept;
	/// 現在掴めるブロックを手動設定する
	void set_grip_target(const std::optional<MapChipIndex>& blockIndex) noexcept;
	/// マップチップの選択と移動可否判定を行う仲介クラスを設定する
	void set_block_movement_judge(Reference<const BlockMovementJudge> judge) noexcept;
	/// Playerが操作する追従カメラを設定
	void set_follow_camera(Reference<FollowCamera> followCamera) noexcept;

	void set_mesh_instance(Reference<szg::SkinningMeshInstance> meshInstance);

private:
	Reference<szg::SkinningMeshInstance> meshInstance_;
	Reference<FollowCamera> followCamera_;
	Reference<const BlockMovementJudge> blockMovementJudge_;

	PlayerInput playerInput_;
	PlayerContext context_;
	PlayerStateManager stateManager_;
};
