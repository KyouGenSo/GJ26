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
	void initialize(PlayerContext& context);
	void update_locomotion_transition(PlayerContext& context);
	void change_state(PlayerState nextState, PlayerContext& context);
	PlayerState select_locomotion_state(const PlayerContext& context) const noexcept;
	IPlayerState& resolve_state(PlayerState state) noexcept;

private:
	PlayerIdleState idleState_;
	PlayerMoveState moveState_;
	PlayerJumpState jumpState_;
	PlayerPushState pushState_;
	PlayerPullState pullState_;

	PlayerState currentState_{ PlayerState::Idle };
	bool isInitialized_{ false };
};
