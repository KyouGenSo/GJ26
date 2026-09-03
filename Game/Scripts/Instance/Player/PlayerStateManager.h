#pragma once

#include "PlayerState.h"

/// <summary>
/// PlayerStateの遷移と実行を管理する
/// </summary>
class PlayerStateManager {
public:
	void update(PlayerContext& context);
	void reset(PlayerContext& context);

	PlayerState get_current_state() const noexcept;

private:
	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="context"></param>
	void initialize(PlayerContext& context);

	/// <summary>
	/// 現在のLocomotion状態(PlayerState::Idle or PlayerState::Move)を更新する
	/// </summary>
	/// <param name="context"></param>
	void update_locomotion_transition(PlayerContext& context);

	/// <summary>
	/// PlayerStateを変更する
	/// </summary>
	/// <param name="nextState"></param>
	/// <param name="context"></param>
	void change_state(PlayerState nextState, PlayerContext& context);

	/// <summary>
	/// 現在の入力状態からLocomotion状態(PlayerState::Idle or PlayerState::Move)を選択する
	/// </summary>
	/// <param name="context"></param>
	/// <returns></returns>
	PlayerState select_locomotion_state(const PlayerContext& context) const noexcept;

	/// <summary>
	/// PlayerStateに対応するIPlayerStateを取得する
	/// </summary>
	/// <param name="state"></param>
	/// <returns></returns>
	IPlayerState& resolve_state(PlayerState state) noexcept;

private:
	PlayerIdleState idleState_;
	PlayerMoveState moveState_;
	PlayerJumpState jumpState_;
	PlayerGripState gripState_;

	PlayerState currentState_{ PlayerState::Idle };
	bool isInitialized_{ false };
};
