#pragma once
#include "Scripts/Instance/Player/State/IPlayerState.h"

/// <summary>
/// プレイヤーのジャンプ状態
/// </summary>
class PlayerJumpState final : public IPlayerState {
public:
	void enter(PlayerContext& context) override;
	void execute(PlayerContext& context) override;
	void exit(PlayerContext& context) override;
};