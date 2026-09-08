#include "GoalEffect.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/Json/JsonAsset.h>
#include <Engine/Loader/EmitterInstanceLoader.h>
#include <Engine/Runtime/Clock/WorldClock.h>
#include <Engine/Runtime/Particle/ParticlePool.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>

namespace {

// StageEditorで調整したエミッタの高さ。Goalモデルは0.5倍なので親ローカル値は2倍する。
constexpr r32 kEmitterLocalY = 1.3120096f * 2.0f;
constexpr r32 kGoalModelInverseScale = 2.0f;
constexpr std::array<const char*, 2> kParticleFiles{
	"[[game]]/GoalEffect.particle",
	"[[game]]/GoalEffect2.particle",
};

} // namespace

void GoalEffect::setup(Reference<MapChipField> field_, Reference<szg::WorldRoot> worldRoot_) {
	field = field_;
	worldRoot = worldRoot_;
	setup_json_asset();
	load_particle_settings();
	grayscaleData = szg::RuntimeStorage::GetValue<Reference<szg::GrayscalePipeline::Data>>(
		"PostEffect", "GoalGrayscale").value_or(nullptr);
	if (!grayscaleData) {
		szgWarning("GoalEffect: GoalGrayscale post effect runtime reference not found.");
	}
}

void GoalEffect::finalize() {
	restore_visual_transform();
	if (grayscaleData) {
		grayscaleData->isGray = 1u;
	}
	destroy_emitters();
	grayscaleData.reset();
	goalVisual.reset();
	goalIndex.reset();
	field.reset();
	worldRoot.reset();
	isActive = false;
}

void GoalEffect::set_goal(const std::optional<MapChipIndex>& goalIndex_, bool active) {
	Reference<szg::StaticMeshInstance> nextVisual =
		field && goalIndex_ ? field->visual_mut(*goalIndex_) : nullptr;
	const bool visualChanged = nextVisual != goalVisual;

	if (visualChanged) {
		restore_visual_transform();
		destroy_emitters();
		goalVisual = nextVisual;
		goalIndex = goalIndex_;
		floatAnimationTime = 0.0f;
		currentYawDegrees = 0.0f;
		activeBlend = 0.0f;
		if (goalVisual) {
			basePosition = goalVisual->transform_imm().get_translate();
			baseRotation = goalVisual->transform_imm().get_quaternion();
			create_emitters();
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
	if (!goalVisual) {
		return;
	}

	const r32 deltaSeconds = szg::WorldClock::DeltaSeconds();
	const r32 transitionStep = deltaSeconds / std::max(stateTransitionDuration, 0.001f);
	activeBlend = isActive
		? std::min(activeBlend + transitionStep, 1.0f)
		: std::max(activeBlend - transitionStep, 0.0f);
	const r32 easedBlend = activeBlend * activeBlend * (3.0f - 2.0f * activeBlend);

	Vector3 position = basePosition;
	if (activeBlend > 0.0f) {
		floatAnimationTime += deltaSeconds;
		const r32 period = std::max(floatPeriod, 0.001f);
		const r32 phase = floatAnimationTime * (2.0f * std::numbers::pi_v<r32> / period);
		position.y += std::sin(phase) * floatAmplitude * easedBlend;
	}
	goalVisual->transform_mut().set_translate(position);

	// OFFは逆回転、ONは正回転。ブレンド中は速度が連続的に反転する。
	const r32 rotationDirection = easedBlend * 2.0f - 1.0f;
	currentYawDegrees = std::remainder(
		currentYawDegrees + rotationSpeedDegrees * rotationDirection * deltaSeconds,
		360.0f);
	goalVisual->transform_mut().set_quaternion(
		baseRotation * Quaternion::EulerDegree(Vector3{ 0.0f, currentYawDegrees, 0.0f }));

	// Emitter.update()はAffine更新より先に呼ばれるため、次フレームの放出位置をここで同期する。
	goalVisual->update_affine();
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
	stateTransitionDuration = std::max(
		readR32("StateTransitionDuration", stateTransitionDuration), 0.001f);
}

void GoalEffect::load_particle_settings() {
	for (size_t i = 0; i < kParticleFiles.size(); ++i) {
		szg::JsonAsset particle{ kParticleFiles[i] };
		emitterSettings[i] = szg::EmitterInstanceLoader::Load(particle.cget());
		if (!emitterSettings[i]) {
			szgWarning("GoalEffect: {} could not be loaded.", kParticleFiles[i]);
		}
	}
}

void GoalEffect::create_emitters() {
	if (!worldRoot || !goalVisual) {
		return;
	}

	for (size_t i = 0; i < emitterSettings.size(); ++i) {
		if (!emitterSettings[i]) {
			continue;
		}
		Reference<szg::EmitterInstance> emitter =
			worldRoot->instantiate<szg::EmitterInstance>(goalVisual);
		emitter->setup_settings(*emitterSettings[i]);
		Reference<szg::ParticlePool> pool = worldRoot->create_particle_pool(
			emitter,
			emitterSettings[i]->capacity == 0 ? 1 : emitterSettings[i]->capacity,
			emitterSettings[i]->overflowPolicy);
		emitter->setup_pool(pool);
		if (pool) {
			pool->setup_draw_spec(emitterSettings[i]->drawSpec);
			pool->setup_updaters(
				szg::EmitterInstance::BuildUpdaterMask(*emitterSettings[i]),
				emitterSettings[i]->rotation.rotationKind);
		}
		emitters[i] = emitter;
	}

	sync_emitter_transforms();
	for (Reference<szg::EmitterInstance> emitter : emitters) {
		if (emitter) {
			emitter->set_active(false);
		}
	}
}

void GoalEffect::destroy_emitters() {
	for (Reference<szg::EmitterInstance>& emitter : emitters) {
		if (!emitter) {
			continue;
		}
		if (Reference<szg::ParticlePool> pool = emitter->pool_mut()) {
			pool->clear();
		}
		emitter->reparent(nullptr, true);
		if (!emitter->is_marked_destroy()) {
			emitter->destroy_self();
		}
		emitter.reset();
	}
}

void GoalEffect::sync_emitter_transforms() {
	for (Reference<szg::EmitterInstance> emitter : emitters) {
		if (!emitter) {
			continue;
		}
		emitter->transform_mut().set_translate(Vector3{ 0.0f, kEmitterLocalY, 0.0f });
		emitter->transform_mut().set_scale(Vector3{
			kGoalModelInverseScale,
			kGoalModelInverseScale,
			kGoalModelInverseScale,
		});
		// 非Activeへ切り替える前に、ゲーム開始時のGoal位置を反映する。
		emitter->update_affine();
	}
}

void GoalEffect::apply_active_state(bool active) {
	const bool wasActive = isActive;
	isActive = active;
	if (isActive && !wasActive && activeBlend <= 0.0f) {
		floatAnimationTime = 0.0f;
	}
	if (grayscaleData) {
		grayscaleData->isGray = isActive ? 0u : 1u;
	}
	if (!isActive) {
		for (Reference<szg::EmitterInstance> emitter : emitters) {
			if (!emitter) {
				continue;
			}
			emitter->set_active(false);
			if (Reference<szg::ParticlePool> pool = emitter->pool_mut()) {
				pool->clear();
			}
		}
		return;
	}

	for (Reference<szg::EmitterInstance> emitter : emitters) {
		if (!emitter) {
			continue;
		}
		emitter->restart_schedule();
		emitter->set_active(true);
	}
	sync_emitter_transforms();
}

void GoalEffect::restore_visual_transform() {
	if (!goalVisual) {
		return;
	}
	goalVisual->transform_mut().set_translate(basePosition);
	goalVisual->transform_mut().set_quaternion(baseRotation);
}
