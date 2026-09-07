#pragma once

#include <Scripts/Instance/Player/PlayerContext.h>

enum class PlayerState {
	Idle,
	Move,
	Jump,
	Grip,
};

/// <summary>
/// PlayerStateの共通インターフェース
/// </summary>
class IPlayerState {
public:
	virtual ~IPlayerState() = default;

	virtual void enter(PlayerContext& context) = 0;
	virtual void execute(PlayerContext& context) = 0;
	virtual void exit(PlayerContext& context) = 0;

	bool is_running() {
		return isRunning_;
	}

protected:
	bool isRunning_{ false };
};
