#pragma once
#include "Scripts/Instance/Player/State/IPlayerState.h"

/// <summary>
/// プレイヤーの待機状態
/// </summary>
class PlayerIdleState final : public IPlayerState {
public:
	void enter(PlayerContext& context) override;
	void execute(PlayerContext& context) override;
	void exit(PlayerContext& context) override;
};