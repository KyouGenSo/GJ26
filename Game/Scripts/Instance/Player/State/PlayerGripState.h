#pragma once
#include "Scripts/Instance/Player/State/IPlayerState.h"

/// <summary>
/// 掴み入力中のState
/// </summary>
class PlayerGripState final : public IPlayerState {
public:
	void enter(PlayerContext& context) override;
	void execute(PlayerContext& context) override;
	void exit(PlayerContext& context) override;
};