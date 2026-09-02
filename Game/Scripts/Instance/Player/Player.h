#pragma once
#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Engine/Module/World/WorldInstance/WorldInstance.h>

#include <Library/Utility/Template/Reference.h>

#include "PlayerInput.h"

enum class PlayerState {
	Idle,
	Move,
};

enum class PlayerAction {
	None,
	Jump,
	push,
	pull,
};

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

public:
	/// <summary>
	/// 操作対象のWorldInstanceを設定
	/// </summary>
	void set_world_instance(Reference<szg::WorldInstance> worldInstance) noexcept;

	/// <summary>
	/// 入力設定を取得
	/// </summary>
	PlayerInput& input_mut() noexcept;

	/// <summary>
	/// 現在フレームの入力を取得
	/// </summary>
	const PlayerInputFrame& input_imm() const noexcept;

	void set_state(PlayerState state);
	PlayerState state_imm() const noexcept;

	/// <summary>
	/// 移動速度を設定（ワールド単位/秒）
	/// </summary>
	void set_move_speed(float moveSpeed) noexcept;

	/// <summary>
	/// 移動速度を取得（ワールド単位/秒）
	/// </summary>
	float move_speed() const noexcept;

	void set_action(PlayerAction action);

	/// <summary>
	/// プレイヤーのY座標から接地状態を更新
	/// </summary>
	void update_grounded(float positionY) noexcept;

	/// <summary>
	/// 接地しているか
	/// </summary>
	bool is_grounded() const noexcept;

private:
	void update_movement();

private:
	Reference<szg::WorldInstance> worldInstance;
	PlayerInput playerInput;
	PlayerInputFrame inputFrame;
	PlayerState state{ PlayerState::Idle };
	float moveSpeed{ 5.0f };
	bool isGrounded{ false };
};
