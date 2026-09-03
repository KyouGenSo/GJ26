#include "PlayerStateManager.h"

void PlayerStateManager::update(PlayerContext& context) {
	if (!isInitialized_) {
		initialize(context);
	}

	update_locomotion_transition(context);

	auto& currentState = resolve_state(currentState_);
	currentState.execute(context);

	if (!currentState.is_running()) {
		change_state(select_locomotion_state(context), context);
	}
}

void PlayerStateManager::reset(PlayerContext& context) {
	if (isInitialized_) {
		resolve_state(currentState_).exit(context);
	}

	currentState_ = PlayerState::Idle;
	isInitialized_ = false;
}

PlayerState PlayerStateManager::get_current_state() const noexcept {
	return currentState_;
}

void PlayerStateManager::initialize(PlayerContext& context) {
	currentState_ = PlayerState::Idle;
	resolve_state(currentState_).enter(context);
	isInitialized_ = true;
}

void PlayerStateManager::update_locomotion_transition(PlayerContext& context) {
	if (currentState_ != PlayerState::Idle && currentState_ != PlayerState::Move) {
		return;
	}

	if (context.input.jumpTriggered && context.isGrounded) {
		change_state(PlayerState::Jump, context);
		return;
	}

	if (context.gripTargetIndex && context.isGrounded && context.input.gripPressed) {
		change_state(PlayerState::Grip, context);
		return;
	}

	const PlayerState locomotionState = select_locomotion_state(context);
	if (currentState_ != locomotionState) {
		change_state(locomotionState, context);
	}
}

void PlayerStateManager::change_state(PlayerState nextState, PlayerContext& context) {
	if (isInitialized_ && currentState_ == nextState) {
		return;
	}

	if (isInitialized_) {
		resolve_state(currentState_).exit(context);
	}

	currentState_ = nextState;
	resolve_state(currentState_).enter(context);
}

PlayerState PlayerStateManager::select_locomotion_state(const PlayerContext& context) const noexcept {
	return context.input.move.length() == 0.0f ? PlayerState::Idle : PlayerState::Move;
}

IPlayerState& PlayerStateManager::resolve_state(PlayerState state) noexcept {
	switch (state) {
	case PlayerState::Idle:
		return idleState_;
	case PlayerState::Move:
		return moveState_;
	case PlayerState::Jump:
		return jumpState_;
	case PlayerState::Grip:
		return gripState_;
	default:
		return idleState_;
	}
}
