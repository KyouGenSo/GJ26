#include "PlayerIdleState.h"

void PlayerIdleState::enter(PlayerContext&) {
	isRunning_ = true;
}

void PlayerIdleState::execute(PlayerContext&) {
}

void PlayerIdleState::exit(PlayerContext&) {
	isRunning_ = false;
}