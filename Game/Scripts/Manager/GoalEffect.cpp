#include "GoalEffect.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/Json/JsonAsset.h>
#include <Engine/Loader/EmitterInstanceLoader.h>
#include <Engine/Runtime/Clock/WorldClock.h>
#include <Engine/Runtime/Particle/ParticlePool.h>

namespace {

// StageEditorで調整したエミッタの高さ。Goalモデルは0.5倍なので親ローカル値は2倍する。
constexpr r32 kEmitterLocalY = 1.3120096f * 2.0f;
constexpr r32 kGoalModelInverseScale = 2.0f;

} // namespace

void GoalEffect::setup(Reference<MapChipField> field_, Reference<szg::WorldRoot> worldRoot_) {
	field = field_;
	worldRoot = worldRoot_;
	setup_json_asset();
	load_particle_settings();
}

void GoalEffect::finalize() {
	restore_visual_transform();
	if (field && goalIndex) {
		field->set_goal_active(*goalIndex, false);
	}
	destroy_emitter();
	goalVisual.reset();
	goalIndex.reset();
	field.reset();
	worldRoot.reset();
	isActive = false;
}

void GoalEffect::set_goal(const std::optional<MapChipIndex>& goalIndex_, bool active) {
	// モデル切替では表示インスタンスが再生成されるため、切替後の参照を取得する。
	if (field && goalIndex_) {
		field->set_goal_active(*goalIndex_, active);
	}
	Reference<szg::StaticMeshInstance> nextVisual =
		field && goalIndex_ ? field->visual_mut(*goalIndex_) : nullptr;
	const bool visualChanged = nextVisual != goalVisual;

	if (visualChanged) {
		restore_visual_transform();
		destroy_emitter();
		goalVisual = nextVisual;
		goalIndex = goalIndex_;
		animationTime = 0.0f;
		if (goalVisual) {
			basePosition = goalVisual->transform_imm().get_translate();
			baseRotation = goalVisual->transform_imm().get_quaternion();
			create_emitter();
		}
	}
	else {
		goalIndex = goalIndex_;
	}

	if (visualChanged || isActive != active) {
		apply_active_state(active && static_cast<bool>(goalVisual));
	}
}

void GoalEffect::update() {
	if (!isActive || !goalVisual) {
		return;
	}

	animationTime += szg::WorldClock::DeltaSeconds();
	const r32 period = std::max(floatPeriod, 0.001f);
	const r32 phase = animationTime * (2.0f * std::numbers::pi_v<r32> / period);

	Vector3 position = basePosition;
	position.y += std::sin(phase) * floatAmplitude;
	goalVisual->transform_mut().set_translate(position);

	const r32 yawDegrees = std::fmod(animationTime * rotationSpeedDegrees, 360.0f);
	goalVisual->transform_mut().set_quaternion(
		baseRotation * Quaternion::EulerDegree(Vector3{ 0.0f, yawDegrees, 0.0f }));
}

void GoalEffect::setup_json_asset() {
	szg::JsonAsset parameter{ "[[game]]/GoalEffect.param" };
	const nlohmann::json& json = parameter.cget();
	if (!json.is_object()) {
		szgWarning("GoalEffect: GoalEffect.param could not be loaded. Default values are used.");
		return;
	}

	const auto readR32 = [&json](const char* name, r32 fallback) {
		return json.value(name, nlohmann::json::object()).value("value", fallback);
	};
	floatAmplitude = std::max(readR32("FloatAmplitude", floatAmplitude), 0.0f);
	floatPeriod = std::max(readR32("FloatPeriod", floatPeriod), 0.001f);
	rotationSpeedDegrees = readR32("RotationSpeedDegrees", rotationSpeedDegrees);
}

void GoalEffect::load_particle_settings() {
	szg::JsonAsset particle{ "[[game]]/GoalEffect.particle" };
	emitterSettings = szg::EmitterInstanceLoader::Load(particle.cget());
	if (!emitterSettings) {
		szgWarning("GoalEffect: GoalEffect.particle could not be loaded.");
	}
}

void GoalEffect::create_emitter() {
	if (!worldRoot || !goalVisual || !emitterSettings) {
		return;
	}

	emitter = worldRoot->instantiate<szg::EmitterInstance>(goalVisual);
	emitter->setup_settings(*emitterSettings);
	Reference<szg::ParticlePool> pool = worldRoot->create_particle_pool(
		emitter,
		emitterSettings->capacity == 0 ? 1 : emitterSettings->capacity,
		emitterSettings->overflowPolicy);
	emitter->setup_pool(pool);
	if (pool) {
		pool->setup_draw_spec(emitterSettings->drawSpec);
		pool->setup_updaters(
			szg::EmitterInstance::BuildUpdaterMask(*emitterSettings),
			emitterSettings->rotation.rotationKind);
	}

	emitter->transform_mut().set_translate(Vector3{ 0.0f, kEmitterLocalY, 0.0f });
	emitter->transform_mut().set_scale(Vector3{
		kGoalModelInverseScale,
		kGoalModelInverseScale,
		kGoalModelInverseScale,
	});
	emitter->update_affine();
	emitter->set_active(false);
}

void GoalEffect::destroy_emitter() {
	if (!emitter) {
		return;
	}
	if (Reference<szg::ParticlePool> pool = emitter->pool_mut()) {
		pool->clear();
	}
	if (!emitter->is_marked_destroy()) {
		emitter->destroy_self();
	}
	emitter.reset();
}

void GoalEffect::apply_active_state(bool active) {
	isActive = active;
	animationTime = 0.0f;
	if (!isActive) {
		restore_visual_transform();
		if (emitter) {
			emitter->set_active(false);
			if (Reference<szg::ParticlePool> pool = emitter->pool_mut()) {
				pool->clear();
			}
		}
		return;
	}

	if (emitter) {
		emitter->restart_schedule();
		emitter->set_active(true);
		emitter->update_affine();
	}
}

void GoalEffect::restore_visual_transform() {
	if (!goalVisual) {
		return;
	}
	goalVisual->transform_mut().set_translate(basePosition);
	goalVisual->transform_mut().set_quaternion(baseRotation);
}
