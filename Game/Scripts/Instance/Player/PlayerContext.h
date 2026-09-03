#pragma once

#include <optional>

#include <Engine/Module/World/WorldInstance/WorldInstance.h>

#include <Library/Utility/Template/Reference.h>

#include "PlayerInputFrame.h"
#include "Scripts/MapChip/BlockMovementJudge.h"

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
	/// ブロックを掴んでいる間の横移動速度
	float gripMoveSpeed{ 2.5f };

	/// Y座標が0のときtrue
	bool isGrounded{ false };
	/// 現在掴めるブロックのインデックス
	std::optional<MapChipIndex> gripTargetIndex;
	/// 現在掴んでいるブロックのインデックス
	std::optional<MapChipIndex> grippedBlockIndex;
	/// 掴んだブロックの前後左右への移動可否
	std::optional<BlockMoveResult> blockMoveResult;

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
	/// 最後に移動したXZ平面上のワールド方向
	Vector3 direction{ 0.0f, 0.0f, 1.0f };
};
