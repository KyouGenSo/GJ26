#pragma once

#include <optional>

#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Module/World/Particle/EmitterInstance.h>
#include <Engine/Runtime/Particle/EmitterSettings.h>
#include <Engine/Runtime/Scene/World/WorldRoot.h>
#include <Library/Math/Quaternion.h>
#include <Library/Math/Vector3.h>
#include <Library/Utility/Template/Reference.h>
#include <Library/Utility/Tools/ConstructorMacro.h>

#include "Scripts/MapChip/MapChipField.h"

/// <summary>
/// ゴールのActive表示、上下移動、回転、パーティクルを管理する
/// </summary>
class GoalEffect final {
public:
	GoalEffect() = default;
	~GoalEffect() = default;

	SZG_CLASS_MOVE_ONLY(GoalEffect)

public:
	void setup(Reference<MapChipField> field_, Reference<szg::WorldRoot> worldRoot_);
	void finalize();

	/// <summary>
	/// 演出対象のGoalセルとActive状態を反映する。nulloptなら演出対象を解除する
	/// </summary>
	void set_goal(const std::optional<MapChipIndex>& goalIndex_, bool active);

	/// <summary>
	/// Active中の上下移動と回転を更新する
	/// </summary>
	void update();

private:
	void setup_json_asset();
	void load_particle_settings();
	void create_emitter();
	void destroy_emitter();
	void apply_active_state(bool active);
	void restore_visual_transform();

private:
	Reference<MapChipField> field;
	Reference<szg::WorldRoot> worldRoot;
	Reference<szg::StaticMeshInstance> goalVisual;
	Reference<szg::EmitterInstance> emitter;
	std::optional<szg::EmitterInstanceSettings> emitterSettings;
	std::optional<MapChipIndex> goalIndex;
	Vector3 basePosition{ CVector3::ZERO };
	Quaternion baseRotation{ CQuaternion::IDENTITY };
	r32 animationTime{ 0.0f };
	r32 floatAmplitude{ 0.1f };
	r32 floatPeriod{ 1.5f };
	r32 rotationSpeedDegrees{ 45.0f };
	bool isActive{ false };
};
