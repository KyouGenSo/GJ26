#pragma once

#include <optional>
#include <vector>

#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Module/World/Particle/EmitterInstance.h>
#include <Engine/Runtime/Particle/EmitterSettings.h>
#include <Engine/Runtime/Scene/World/WorldRoot.h>
#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Library/Utility/Template/Reference.h>

#include "GoalEffect.h"
#include "Scripts/MapChip/MapChipField.h"

class Player;

/// <summary>
/// <para>ゴール条件オブジェクトの接続判定と、ゴールの出現・クリア判定</para>
/// <para>同じ高さで X か Z 方向に並び、間に障害物(粘土・別ピース)が無いピース同士がつながる</para>
/// <para>全ピースが 1 つにつながるとゴールを表示し、そのセルにプレイヤーが入るとクリア</para>
/// </summary>
class GoalManager final : public szg::ISceneScript {
public:
	GoalManager() = default;
	~GoalManager() = default;

	SZG_CLASS_MOVE_ONLY(GoalManager)

public:
	void setup(Reference<MapChipField> field_, Reference<szg::WorldRoot> worldRoot_);

	/// <summary>
	/// クリア判定に使うプレイヤー
	/// </summary>
	void set_player(Reference<const Player> player_);

	void finalize() override;
	void post_update() override;

	/// <summary>
	/// 全ピースが接続済みでゴールが出現している
	/// </summary>
	bool is_goal_open() const { return goalOpen; }

	/// <summary>
	/// 出現したゴールのセルにプレイヤーがいるか
	/// </summary>
	bool is_cleared() const { return cleared; }

private:
	void rebuild();
	bool is_connected(const MapChipIndex& a, const MapChipIndex& b) const;
	void create_link(const MapChipIndex& a, const MapChipIndex& b);
	void destroy_links();

	/// <summary>
	/// 接続中ピースの星から出すパーティクルを、ピースの表示インスタンスの子として生成する
	/// </summary>
	void create_piece_emitter(const MapChipIndex& index);
	void destroy_piece_emitters();

private:
	Reference<MapChipField> field;
	Reference<szg::WorldRoot> worldRoot;
	Reference<const Player> player;
	GoalEffect goalEffect;
	std::vector<Reference<szg::StaticMeshInstance>> links; // ピース間の線
	std::vector<Reference<szg::EmitterInstance>> pieceEmitters; // 接続中ピースの星のパーティクル
	std::optional<szg::EmitterInstanceSettings> pieceEmitterSettings;
	std::optional<MapChipIndex> goal;
	u32 lastVersion{ 0 };
	bool rebuildPending{ false }; // 移動補間が終わってから再判定する
	bool goalOpen{ false };
	bool cleared{ false };
};
