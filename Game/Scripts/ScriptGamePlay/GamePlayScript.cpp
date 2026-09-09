#include "GamePlayScript.h"

#include <algorithm>
#include <cmath>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/Json/JsonAsset.h>
#include <Engine/Loader/EmitterInstanceLoader.h>
#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Module/World/Mesh/Primitive/StringRectInstance.h>
#include <Engine/Module/World/Particle/EmitterInstance.h>
#include <Engine/Module/World/Mesh/Primitive/Rect3d.h>
#include <Engine/Module/World/Mesh/SkinningMeshInstance.h>
#include <Engine/Runtime/Clock/WorldClock.h>
#include <Engine/Runtime/Particle/ParticlePool.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Runtime/Scene/SceneManager2.h>
#include <Engine/Runtime/Scene/World/WorldRoot.h>
#include <Library/Utility/Tools/SmartPointer.h>

#include "Scripts/Instance/FollowCamera/FollowCamera.h"
#include "Scripts/Instance/Player/Player.h"
#include "Scripts/Manager/GoalManager.h"
#include "Scripts/Manager/UndoManager.h"
#include "Scripts/Scene/FactoryGJ26.h"
#include "Scripts/ScriptMapTest/MapTestScript.h"

#ifdef DEBUG_FEATURES_ENABLE
#include <imgui.h>
#endif // DEBUG_FEATURES_ENABLE

namespace {

constexpr r32 kBackHoldDurationSeconds = 1.0f;
constexpr r32 kResetHoldDurationSeconds = 1.0f;
constexpr r32 kClayGlowWeightDefault = 0.3f;

} // namespace

void GamePlayScript::setup(Reference<szg::WorldRoot> worldRoot) {
	if (isSetup_) {
		szgWarning("GamePlayScript: setup was called more than once.");
		return;
	}
	if (!worldRoot) {
		szgError("GamePlayScript: WorldRoot not found.");
		return;
	}
	worldRoot_ = worldRoot;
	setup_json_asset();
	setup_clear_presentation();
	keyInput_.initialize({ szg::KeyID::Escape }, szg::InputInitializeMode::Current);
	padInput_.initialize({ szg::PadID::Start, szg::PadID::Y }, szg::InputInitializeMode::Current);

	std::unique_ptr<MapTestScript> mapTest = eps::CreateUnique<MapTestScript>();
	mapTest_ = mapTest;
	// Goalだけを専用レイヤーへ描画し、Grayscale後に通常レイヤーと合成する。
	mapTest_->field_mut().set_goal_visual_layer(1);
	mapTest_->setup(worldRoot);

	const auto playerInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::WorldInstance>>("RuntimeInstance", "Player");
	const auto playerMeshInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::SkinningMeshInstance>>("RuntimeInstance", "PlayerMesh");
	const auto cameraInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::CameraInstance>>("RuntimeInstance", "MainCamera");
	const auto cameraFollowTargetInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::WorldInstance>>("RuntimeInstance", "CameraFollowTarget");

	// プレイヤーのスクリプト
	std::unique_ptr<Player> player = 
		eps::CreateUnique<Player>(playerInstance.value_or(nullptr),playerMeshInstance.value_or(nullptr));

	player_ = player;
	if (playerInstance) {
		player_->set_world_instance(playerInstance.value_or(nullptr));
	}
	else {
		szgWarning("GamePlayScript: Player runtime instance not found.");
	}
	player_->set_block_movement_judge(mapTest_->movement_judge_mut());

	std::unique_ptr<FollowCamera> followCamera;
	if (cameraInstance && cameraFollowTargetInstance) {
		followCamera = eps::CreateUnique<FollowCamera>(
			cameraInstance.value_or(nullptr),
			cameraFollowTargetInstance.value_or(nullptr));
		followCamera_ = followCamera;
		player_->set_follow_camera(followCamera_);
		mapTest_->set_follow_camera(followCamera_);
	}
	else if (!cameraInstance) {
		szgWarning("GamePlayScript: MainCamera runtime instance not found.");
	}
	else {
		szgWarning("GamePlayScript: CameraFollowTarget runtime instance not found.");
	}

	mapTest_->set_player(player_);

	std::unique_ptr<UndoManager> undoManager = eps::CreateUnique<UndoManager>();
	undoManager_ = undoManager;
	undoManager_->setup(mapTest_->field_mut(), player_);
	mapTest_->set_undo_manager(undoManager_);

	std::unique_ptr<GoalManager> goalManager = eps::CreateUnique<GoalManager>();
	goalManager_ = goalManager;
	goalManager_->setup(mapTest_->field_mut(), worldRoot);
	goalManager_->set_player(player_);

	// Undo -> ステージ更新 -> Player移動 -> 追従カメラ更新 -> ゴール判定の順に実行する
	inGameScriptManager_.register_script(std::move(undoManager));
	inGameScriptManager_.register_script(std::move(mapTest));
	inGameScriptManager_.register_script(std::move(player));
	if (followCamera) {
		inGameScriptManager_.register_script(std::move(followCamera));
	}
	inGameScriptManager_.register_script(std::move(goalManager));

	// Bloom の既定 weight(1.0)は粘土の質感が飛ぶので ClayGlow.param の値に下げる
	clayGlow_ = szg::RuntimeStorage::GetValue<Reference<szg::BloomPipeline::Data>>("PostEffect", "ClayGlow").value_or(nullptr);
	if (clayGlow_) {
		szg::JsonAsset parameter{ "[[game]]/ClayGlow.param" };
		clayGlow_->weight = parameter.cget().value("Weight", nlohmann::json::object()).value("value", kClayGlowWeightDefault);
	}
	else {
		szgWarning("GamePlayScript: ClayGlow bloom not found.");
	}

	resetGaugeFill_ = szg::RuntimeStorage::GetValue<Reference<szg::Rect3d>>("RuntimeInstance", "ResetGaugeFill").value_or(nullptr);
	if (resetGaugeFill_) {
		resetGaugeFullWidth_ = resetGaugeFill_->data_imm().size.x;
	}
	else {
		szgWarning("GamePlayScript: ResetGaugeFill runtime instance not found.");
	}

	// 掴める対象の輪郭の色と太さ
	{
		szg::JsonAsset parameter{ "[[game]]/GripHighlight.param" };
		const nlohmann::json& json = parameter.cget();
		const auto readR32 = [&json](const char* name, r32 fallback) {
			return json.value(name, nlohmann::json::object()).value("value", fallback);
		};
		gripHighlight_.color.red = readR32("ColorR", gripHighlight_.color.red);
		gripHighlight_.color.green = readR32("ColorG", gripHighlight_.color.green);
		gripHighlight_.color.blue = readR32("ColorB", gripHighlight_.color.blue);
		gripHighlight_.thickness = readR32("Thickness", gripHighlight_.thickness);
		mapTest_->field_mut().set_highlight_style(gripHighlight_);
	}

	isSetup_ = true;
}

void GamePlayScript::finalize() {
	if (!isSetup_) {
		return;
	}

	destroy_confetti_emitters();
	inGameScriptManager_.finalize();
	mapTest_.reset();
	player_.reset();
	followCamera_.reset();
	goalManager_.reset();
	undoManager_.reset();
	clearText_.reset();
	worldRoot_.reset();
	confettiSettings_.reset();
	isSetup_ = false;
	clayGlow_.reset();
	resetGaugeFill_.reset();
}

void GamePlayScript::prev_update() {
	if (!isSetup_) {
		return;
	}
	keyInput_.update();
	padInput_.update();

	// EscapeキーまたはStartボタンが一定時間押され続けた場合、ステージ選択画面へ遷移する
	const r32 backHoldSeconds = std::max(
		keyInput_.press_timer(szg::KeyID::Escape),
		padInput_.press_timer(szg::PadID::Start));
	if (!sceneTransitionRequested_ && backHoldSeconds >= kBackHoldDurationSeconds) {
		sceneTransitionRequested_ = true;

		// ステージ選択画面へ遷移する
		szg::SceneManager2::SceneChange(SceneListGJ26::Select, 0.0f);
		return;
	}

	// Yボタンが一定時間押され続けた場合、ステージを初期状態に戻す
	if (padInput_.release(szg::PadID::Y)) {
		resetHoldConsumed_ = false;
	}
	if (!resetHoldConsumed_ && padInput_.press_timer(szg::PadID::Y) >= kResetHoldDurationSeconds) {
		resetHoldConsumed_ = true;
		reset_clear_sequence();
		mapTest_->reload();
	}

	// Y 長押し中はリセットゲージを左から伸ばす(離すと 0 に戻る)
	if (resetGaugeFill_) {
		const r32 ratio = std::clamp(padInput_.press_timer(szg::PadID::Y) / kResetHoldDurationSeconds, 0.0f, 1.0f);
		resetGaugeFill_->data_mut().size.x = resetGaugeFullWidth_ * ratio;
		resetGaugeFill_->material_mut().uvTransform.set_scale(Vector2{ ratio, 1.0f });
	}

	inGameScriptManager_.prev_update();

#ifdef DEBUG_FEATURES_ENABLE
	if (clayGlow_) {
		ImGui::Begin("ClayGlow");
		ImGui::DragFloat("Weight", &clayGlow_->weight, 0.01f, 0.0f, 2.0f);
		ImGui::End();
	}
	{
		ImGui::Begin("GripHighlight");
		bool changed = ImGui::ColorEdit3("Color", &gripHighlight_.color.red);
		changed |= ImGui::DragFloat("Thickness", &gripHighlight_.thickness, 0.005f, 0.0f, 0.5f);
		if (changed) {
			mapTest_->field_mut().set_highlight_style(gripHighlight_);
		}
		ImGui::End();
	}
#endif // DEBUG_FEATURES_ENABLE

	// ポーズなど、各要素の更新後に行うインゲーム全体処理をここへ追加する。
}

void GamePlayScript::post_update() {
	if (!isSetup_) {
		return;
	}

	inGameScriptManager_.post_update();

	// 目の前の掴める対象(Grip 中は掴んでいるブロック)に輪郭を出す
	if (player_) {
		const std::optional<MapChipIndex>& gripped = player_->get_gripped_block_index();
		mapTest_->field_mut().set_highlight(gripped ? gripped : player_->get_grip_target_index());
	}

	if (!clearCameraEffectStarted_ && goalManager_ && goalManager_->is_cleared() &&
		followCamera_ && player_) {
	if (!clearSequenceStarted_ && goalManager_ && goalManager_->is_cleared() && player_) {
		clearSequenceStarted_ = true;
		player_->set_input_enabled(false);

		const Reference<const szg::WorldInstance> playerInstance = player_->get_world_instance_imm();
		if (playerInstance) {
			goalClearEffectStarted_ = goalManager_->start_clear_effect(playerInstance->world_position());
			Vector3 targetPosition = playerInstance->world_position();
			targetPosition.y += clearCameraTargetHeight_;
			if (followCamera_) {
				clearCameraEffectStarted_ = followCamera_->start_goal_effect(
					targetPosition,
					clearCameraDuration_,
					clearCameraDistance_,
					clearCameraElevationDegrees_,
					clearCameraBounceStrength_);
			}
		}
	}

	const bool cameraFinished = !clearCameraEffectStarted_ ||
		(followCamera_ && followCamera_->is_goal_effect_finished());
	const bool goalFinished = !goalClearEffectStarted_ ||
		(goalManager_ && goalManager_->is_clear_effect_finished());
	if (clearSequenceStarted_ && !clearPresentationStarted_ && cameraFinished && goalFinished) {
		clearPresentationStarted_ = true;
		start_confetti_effect();
		start_clear_ui();
	}
	if (clearPresentationStarted_) {
		update_clear_ui();
	}
}

bool GamePlayScript::is_clear_camera_effect_finished() const noexcept {
	return clearCameraEffectStarted_ && followCamera_ && followCamera_->is_goal_effect_finished();
}

void GamePlayScript::setup_json_asset() {
	szg::JsonAsset parameter{ "[[game]]/GamePlay.param" };
	const nlohmann::json& json = parameter.cget();
	if (!json.is_object()) {
		szgWarning("GamePlayScript: GamePlay.param could not be loaded. Default values are used.");
		return;
	}

	const auto readR32 = [&json](const char* name, r32 fallback) {
		return json.value(name, nlohmann::json::object()).value("value", fallback);
	};
	clearCameraDuration_ = std::max(readR32("ClearCameraDuration", clearCameraDuration_), 0.001f);
	clearCameraDistance_ = std::max(readR32("ClearCameraDistance", clearCameraDistance_), 0.1f);
	clearCameraElevationDegrees_ = std::clamp(
		readR32("ClearCameraElevationDegrees", clearCameraElevationDegrees_), 0.0f, 89.0f);
	clearCameraTargetHeight_ = readR32("ClearCameraTargetHeight", clearCameraTargetHeight_);
	clearCameraBounceStrength_ = std::max(
		readR32("ClearCameraBounceStrength", clearCameraBounceStrength_), 0.0f);
	confettiHorizontalOffset_ = std::max(
		readR32("ConfettiHorizontalOffset", confettiHorizontalOffset_), 0.0f);
	confettiVerticalOffset_ = readR32("ConfettiVerticalOffset", confettiVerticalOffset_);
	clearTextStartX_ = readR32("ClearTextStartX", clearTextStartX_);
	clearTextTargetX_ = readR32("ClearTextTargetX", clearTextTargetX_);
	clearTextSlideDuration_ = std::max(
		readR32("ClearTextSlideDuration", clearTextSlideDuration_), 0.001f);
}

void GamePlayScript::setup_clear_presentation() {
	clearText_ = szg::RuntimeStorage::GetValue<Reference<szg::StringRectInstance>>(
		"RuntimeInstance", "StageClearText").value_or(nullptr);
	if (clearText_) {
		Vector3 position = clearText_->transform_imm().get_translate();
		position.x = clearTextStartX_;
		clearText_->transform_mut().set_translate(position);
		clearText_->set_draw(false);
	}
	else {
		szgWarning("GamePlayScript: StageClearText runtime instance not found.");
	}

	szg::JsonAsset particle{ "[[game]]/confettiEffect.particle" };
	confettiSettings_ = szg::EmitterInstanceLoader::Load(particle.cget());
	if (!confettiSettings_) {
		szgWarning("GamePlayScript: confettiEffect.particle could not be loaded.");
		return;
	}
	// クリア時の一度だけの紙吹雪として使う。
	confettiSettings_->schedule.infinite = false;
	confettiSettings_->schedule.cycles = 1;
	create_confetti_emitters();
}

void GamePlayScript::create_confetti_emitters() {
	if (!worldRoot_ || !confettiSettings_) {
		return;
	}
	for (Reference<szg::EmitterInstance>& emitter : confettiEmitters_) {
		emitter = worldRoot_->instantiate<szg::EmitterInstance>(nullptr);
		emitter->setup_settings(*confettiSettings_);
		Reference<szg::ParticlePool> pool = worldRoot_->create_particle_pool(
			emitter,
			confettiSettings_->capacity == 0 ? 1 : confettiSettings_->capacity,
			confettiSettings_->overflowPolicy);
		emitter->setup_pool(pool);
		if (pool) {
			pool->setup_draw_spec(confettiSettings_->drawSpec);
			pool->setup_updaters(
				szg::EmitterInstance::BuildUpdaterMask(*confettiSettings_),
				confettiSettings_->rotation.rotationKind);
		}
		emitter->set_active(false);
	}
}

void GamePlayScript::destroy_confetti_emitters() {
	for (Reference<szg::EmitterInstance>& emitter : confettiEmitters_) {
		if (!emitter) {
			continue;
		}
		if (Reference<szg::ParticlePool> pool = emitter->pool_mut()) {
			pool->clear();
		}
		if (!emitter->is_marked_destroy()) {
			emitter->destroy_self();
		}
		emitter.reset();
	}
}

void GamePlayScript::start_confetti_effect() {
	if (!confettiSettings_ || !followCamera_ || !player_) {
		return;
	}
	const Reference<szg::CameraInstance> camera = followCamera_->get_camera_instance_mut();
	const Reference<const szg::WorldInstance> playerInstance = player_->get_world_instance_imm();
	if (!camera || !playerInstance) {
		return;
	}

	Vector3 focus = playerInstance->world_position();
	focus.y += clearCameraTargetHeight_;
	const Vector3 forward = (focus - camera->transform_imm().get_translate()).normalize_safe(CVector3::BASIS_Z);
	const Vector3 right = Vector3::Cross(CVector3::BASIS_Y, forward).normalize_safe(CVector3::BASIS_X);
	const Vector3 up = Vector3::Cross(forward, right).normalize_safe(CVector3::BASIS_Y);
	const Vector3 center = focus + up * confettiVerticalOffset_;

	for (size_t i = 0; i < confettiEmitters_.size(); ++i) {
		Reference<szg::EmitterInstance> emitter = confettiEmitters_[i];
		if (!emitter) {
			continue;
		}
		const r32 side = i == 0 ? -1.0f : 1.0f;
		szg::EmitterInstanceSettings settings = *confettiSettings_;
		// 画面の外側から中央方向へ紙吹雪を飛ばす。
		settings.emissionShape.coneDirection = right * -side;
		emitter->setup_settings(settings);
		emitter->transform_mut().set_translate(
			center + right * (confettiHorizontalOffset_ * side));
		emitter->update_affine();
		emitter->restart_schedule();
		emitter->set_active(true);
	}
}

void GamePlayScript::start_clear_ui() {
	clearTextElapsed_ = 0.0f;
	if (!clearText_) {
		return;
	}
	Vector3 position = clearText_->transform_imm().get_translate();
	position.x = clearTextStartX_;
	clearText_->transform_mut().set_translate(position);
	clearText_->set_draw(true);
}

void GamePlayScript::update_clear_ui() {
	if (!clearText_) {
		return;
	}
	clearTextElapsed_ += std::max(szg::WorldClock::DeltaSeconds(), 0.0f);
	const r32 t = std::clamp(clearTextElapsed_ / clearTextSlideDuration_, 0.0f, 1.0f);
	const r32 eased = 1.0f - std::pow(1.0f - t, 3.0f);
	Vector3 position = clearText_->transform_imm().get_translate();
	position.x = clearTextStartX_ + (clearTextTargetX_ - clearTextStartX_) * eased;
	clearText_->transform_mut().set_translate(position);
}

void GamePlayScript::reset_clear_sequence() {
	clearSequenceStarted_ = false;
	clearCameraEffectStarted_ = false;
	goalClearEffectStarted_ = false;
	clearPresentationStarted_ = false;
	clearTextElapsed_ = 0.0f;
	if (followCamera_) {
		followCamera_->stop_goal_effect();
	}
	if (goalManager_) {
		goalManager_->stop_clear_effect();
	}
	if (player_) {
		player_->set_input_enabled(true);
	}
	if (clearText_) {
		Vector3 position = clearText_->transform_imm().get_translate();
		position.x = clearTextStartX_;
		clearText_->transform_mut().set_translate(position);
		clearText_->set_draw(false);
	}
	for (Reference<szg::EmitterInstance> emitter : confettiEmitters_) {
		if (!emitter) {
			continue;
		}
		emitter->set_active(false);
		emitter->restart_schedule();
		if (Reference<szg::ParticlePool> pool = emitter->pool_mut()) {
			pool->clear();
		}
	}
}
