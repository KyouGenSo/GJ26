#pragma once

#include "PlayerContext.h"

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

	bool is_running() const noexcept;

protected:
	bool isRunning_{ false };
};

class PlayerIdleState final : public IPlayerState {
public:
	void enter(PlayerContext& context) override;
	void execute(PlayerContext& context) override;
	void exit(PlayerContext& context) override;
};

class PlayerMoveState final : public IPlayerState {
public:
	void enter(PlayerContext& context) override;
	void execute(PlayerContext& context) override;
	void exit(PlayerContext& context) override;
};

class PlayerJumpState final : public IPlayerState {
public:
	void enter(PlayerContext& context) override;
	void execute(PlayerContext& context) override;
	void exit(PlayerContext& context) override;

private:
	float verticalVelocity_{ 0.0f };
};

/// <summary>
/// 掴み入力中のState
/// </summary>
class PlayerGripState final : public IPlayerState {
public:
	void enter(PlayerContext& context) override;
	void execute(PlayerContext& context) override;
	void exit(PlayerContext& context) override;
};
