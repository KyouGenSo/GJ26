#pragma once

#include "PlayerContext.h"

enum class PlayerState {
	Idle,
	Move,
	Jump,
	Push,
	Pull,
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
/// Push、Pullに共通する掴みState
/// 直接使用せず、派生Stateを使用する
/// </summary>
class PlayerGripState : public IPlayerState {
public:
	void enter(PlayerContext& context) override final;
	void execute(PlayerContext& context) override final;
	void exit(PlayerContext& context) override final;

protected:
	virtual bool is_grip_input_active(const PlayerContext& context) const noexcept = 0;
	virtual void execute_grip(PlayerContext& context) = 0;
	virtual void exit_grip(PlayerContext& context);
};

class PlayerPushState final : public PlayerGripState {
protected:
	bool is_grip_input_active(const PlayerContext& context) const noexcept override;
	void execute_grip(PlayerContext& context) override;
};

class PlayerPullState final : public PlayerGripState {
protected:
	bool is_grip_input_active(const PlayerContext& context) const noexcept override;
	void execute_grip(PlayerContext& context) override;
};
