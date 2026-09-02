#pragma once

#include <Engine/Module/World/WorldInstance/WorldInstance.h>

#include <Library/Utility/Template/Reference.h>

#include "PlayerInputFrame.h"

/// <summary>
/// PlayerのStateが参照する共通情報
/// </summary>
struct PlayerContext {
	/// 通常時の横移動速度
	float moveSpeed{ 5.0f };
	/// ジャンプ開始時の上方向速度
	float jumpPower{ 8.0f };
	/// 1秒あたりに加える落下速度
	float fallSpeed{ 20.0f };
	/// Push、Pull中の横移動速度
	float gripMoveSpeed{ 2.5f };

	/// Y座標が0のときtrue
	bool isGrounded{ false };
	/// 掴める対象が存在するときtrue
	bool canGrip{ false };

	/// Stateが操作するPlayerのWorldInstance
	Reference<szg::WorldInstance> worldInstance;
	/// 現在フレームの入力
	PlayerInputFrame input;
	/// 現在フレームの経過秒数
	float deltaSeconds{ 0.0f };
	/// カメラ視点をXZ平面へ投影した前方向
	Vector3 moveForward{ 0.0f, 0.0f, 1.0f };
	/// カメラ視点をXZ平面へ投影した右方向
	Vector3 moveRight{ 1.0f, 0.0f, 0.0f };
};
