#pragma once

#include <array>
#include <optional>

#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Module/World/Particle/EmitterInstance.h>
#include <Engine/Module/Render/RenderPipeline/Posteffect/Grayscale/GrayscalePipeline.h>
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
	/// 回転を常時更新し、Active中だけ上下移動を更新する
	/// </summary>
	void update();

	/// クリア時にGoal本体を上空へ上げ、その後プレイヤーの前へ移動する
	bool start_clear_effect(
		const Vector3& playerWorldPosition,
		const Vector3& playerDirection);
	/// クリア時のGoal移動パラメータを設定する
	void set_clear_effect_parameters(
		const Vector3& finalPlayerOffset,
		const Vector3& finalScale,
		r32 riseHeight,
		r32 riseDuration,
		r32 fallDuration) noexcept;
	/// クリア移動を解除し、Goalをステージ上の基準位置へ戻す
	void stop_clear_effect();
	/// Goalがプレイヤーの前へ到着済みか
	bool is_clear_effect_finished() const noexcept;

private:
	void setup_json_asset();
	void load_particle_settings();
	void create_emitters();
	void destroy_emitters();
	void destroy_glow();
	void sync_emitter_transforms();
	void apply_active_state(bool active);
	void restore_visual_transform();
	Vector3 to_goal_parent_local(const Vector3& worldPosition) const;

	struct ClearMotion {
		Vector3 startPosition{ CVector3::ZERO };
		Vector3 peakPosition{ CVector3::ZERO };
		Vector3 destinationPosition{ CVector3::ZERO };
		Vector3 startScale{ 0.5f, 0.5f, 0.5f };
		r32 elapsed{ 0.0f };
		bool finished{ false };
	};

private:
	Reference<MapChipField> field;
	Reference<szg::WorldRoot> worldRoot;
	Reference<szg::StaticMeshInstance> goalVisual;
	/// 解放中だけ goalVisual の子として持つブルーム用の複製(MapChipField::AttachGlow)
	Reference<szg::StaticMeshInstance> glowVisual;
	std::array<Reference<szg::EmitterInstance>, 2> emitters;
	Reference<szg::GrayscalePipeline::Data> grayscaleData;
	std::array<std::optional<szg::EmitterInstanceSettings>, 2> emitterSettings;
	std::optional<MapChipIndex> goalIndex;
	Vector3 basePosition{ CVector3::ZERO };
	Vector3 baseScale{ 0.5f, 0.5f, 0.5f };
	Quaternion baseRotation{ CQuaternion::IDENTITY };
	r32 floatAnimationTime{ 0.0f };
	r32 currentYawDegrees{ 0.0f };
	r32 activeBlend{ 0.0f };
	r32 floatAmplitude{ 0.1f };
	r32 floatPeriod{ 1.5f };
	r32 rotationSpeedDegrees{ 45.0f };
	r32 stateTransitionDuration{ 0.4f };
	r32 clearRiseHeight{ 3.0f };
	r32 clearRiseDuration{ 0.55f };
	r32 clearFallDuration{ 0.75f };
	/// プレイヤー基準のローカル座標(X=右、Y=上、Z=前)
	Vector3 clearFinalPlayerOffset{ 0.0f, 0.0f, 1.5f };
	/// 降下完了時のモデルのローカルスケール(倍率ではなく絶対値)
	Vector3 clearFinalScale{ 0.5f, 0.5f, 0.5f };
	std::optional<ClearMotion> clearMotion;
	bool isActive{ false };
};
