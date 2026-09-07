#pragma once
#include "Scripts/Instance/Player/State/IPlayerState.h"

/// <summary>
/// プレイヤーの移動状態
/// </summary>
class PlayerMoveState final : public IPlayerState {
public:
	void enter(PlayerContext& context) override;
	void execute(PlayerContext& context) override;
	void exit(PlayerContext& context) override;
};