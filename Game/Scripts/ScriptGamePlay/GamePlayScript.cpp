#include "GamePlayScript.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

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

namespace {

constexpr r32 kBackHoldDurationSeconds = 1.0f;
constexpr r32 kResetHoldDurationSeconds = 1.0f;
constexpr r32 kClayGlowWeightDefault = 0.3f;
constexpr std::array<const char*, 5> kGameplayUiNames{
	"ToSelectLegend",
	"ButtonLegend",
	"UndoResetLegend",
	"ResetGaugeBack",
	"ResetGaugeFill",
};
constexpr std::array<const char*, 2> kConfettiEmitterNames{
	"ConfettiLeftEmitter",
	"ConfettiRightEmitter",
};
/// インゲームで使う音。BGM と移動音はループ、戻る音はシーン遷移をまたいで鳴らす
constexpr std::array<string_literal, 16> kSounds{
	"gameBgm.wav", "clearBgm.wav", "back.wav", "reset.wav", "undo.wav",
	"move.wav", "jump.wav", "grab.wav", "cantGrab.wav",
	"stretch.wav", "clayConnect.wav", "cantMove.wav", "objectMove.wav", "objectFall.wav", "goalConnect.wav", "goal.wav",
};

} // namespace

void GamePlayScript::RegisterAudioAssets() {
	SoundPlayer::RegisterLoadQue(kSounds);
}

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
	sound_.initialize(kSounds);
	sound_.play("gameBgm.wav");

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
	player_->set_sound(sound_);

	std::unique_ptr<FollowCamera> followCamera;
	if (cameraInstance && cameraFollowTargetInstance) {
		followCamera = eps::CreateUnique<FollowCamera>(
			cameraInstance.value_or(nullptr),
			cameraFollowTargetInstance.value_or(nullptr));
		followCamera_ = followCamera;
		constexpr r32 kDegreesToRadians = std::numbers::pi_v<r32> / 180.0f;
		followCamera_->set_rotation(
			cameraInitialYawDegrees_ * kDegreesToRadians,
			cameraInitialPitchDegrees_ * kDegreesToRadians);
		mapTest_->set_camera_framing_parameters(cameraInitialDistance_, cameraFitPadding_);
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
	undoManager_->set_sound(sound_);
	mapTest_->set_undo_manager(undoManager_);

	std::unique_ptr<GoalManager> goalManager = eps::CreateUnique<GoalManager>();
	goalManager_ = goalManager;
	goalManager_->setup(mapTest_->field_mut(), worldRoot);
	goalManager_->set_clear_effect_parameters(
		goalFinalPlayerOffset_,
		goalClearRiseHeight_,
		goalClearRiseDuration_,
		goalClearFallDuration_);
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

	for (size_t i = 0; i < kGameplayUiNames.size(); ++i) {
		gameplayUi_[i] = szg::RuntimeStorage::GetValue<Reference<szg::Rect3d>>(
			"RuntimeInstance", kGameplayUiNames[i]).value_or(nullptr);
		if (!gameplayUi_[i]) {
			szgWarning("GamePlayScript: {} runtime instance not found.", kGameplayUiNames[i]);
		}
	}
	resetGaugeFill_ = gameplayUi_.back();
	if (resetGaugeFill_) {
		resetGaugeFullWidth_ = resetGaugeFill_->data_imm().size.x;
	}
	set_gameplay_ui_visible(true);

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

	stop_confetti_effect();
	inGameScriptManager_.finalize();
	mapTest_.reset();
	player_.reset();
	followCamera_.reset();
	goalManager_.reset();
	undoManager_.reset();
	clearText_.reset();
	gameplayUi_.fill(nullptr);
	confettiEmitters_.fill(nullptr);
	isSetup_ = false;
	clayGlow_.reset();
	resetGaugeFill_.reset();
	clearSequenceStarted_ = false;
	clearCameraEffectStarted_ = false;
	goalClearEffectStarted_ = false;
	clearPresentationStarted_ = false;
	confettiEffectStarted_ = false;
}

void GamePlayScript::prev_update() {
	if (!isSetup_) {
		return;
	}
	keyInput_.update();
	padInput_.update();

	// Startボタンが押され続けたらステージ選択画面へ遷移する
	if (!sceneTransitionRequested_ && padInput_.trigger(szg::PadID::Start)) {
		sceneTransitionRequested_ = true;
		SoundPlayer::PlayAcrossScene("back.wav");

		// ステージ選択画面へ遷移する
		szg::SceneManager2::SceneChange(SceneListGJ26::Select, 0.0f);
		return;
	}

	// Yボタンが一定時間押され続けた場合、ステージを初期状態に戻す(押している間はリセット音が鳴る)
	if (padInput_.trigger(szg::PadID::Y)) {
		sound_.restart("reset.wav");
	}
	if (padInput_.release(szg::PadID::Y)) {
		resetHoldConsumed_ = false;
		sound_.stop("reset.wav");
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

	// ポーズなど、各要素の更新後に行うインゲーム全体処理をここへ追加する。
}

void GamePlayScript::post_update() {
	if (!isSetup_) {
		return;
	}

	inGameScriptManager_.post_update();

	// ゴール出現の立ち上がりで接続音とクリア可能 BGM、消えたら BGM だけ止める。クリアの立ち上がりでゴール音
	if (goalManager_) {
		const bool goalOpen = goalManager_->is_goal_open();
		if (goalOpen && !wasGoalOpen_) {
			sound_.restart("goalConnect.wav");
			sound_.play("clearBgm.wav");
		}
		else if (!goalOpen && wasGoalOpen_) {
			sound_.stop("clearBgm.wav");
		}
		wasGoalOpen_ = goalOpen;

		const bool cleared = goalManager_->is_cleared();
		if (cleared && !wasCleared_) {
			sound_.restart("goal.wav");
		}
		wasCleared_ = cleared;
	}

	// 目の前の掴める対象(Grip 中は掴んでいるブロック)に輪郭を出す
	if (player_) {
		const std::optional<MapChipIndex>& gripped = player_->get_gripped_block_index();
		mapTest_->field_mut().set_highlight(gripped ? gripped : player_->get_grip_target_index());
	}

	if (!clearSequenceStarted_ && goalManager_ && goalManager_->is_cleared() && player_) {
		clearSequenceStarted_ = true;
		clearPresentationStarted_ = true;
		player_->set_input_enabled(false);
		set_gameplay_ui_visible(false);
		start_clear_ui();

		const Reference<const szg::WorldInstance> playerInstance = player_->get_world_instance_imm();
		if (playerInstance) {
			const Vector3 playerPosition = playerInstance->world_position();
			goalClearEffectStarted_ = goalManager_->start_clear_effect(playerPosition);
			if (followCamera_) {
				clearCameraEffectStarted_ = followCamera_->start_goal_effect(
					playerPosition + clearCameraTargetOffset_,
					playerPosition + clearCameraFinalOffset_,
					clearCameraDuration_,
					clearCameraBounceStrength_);
			}
		}
	}

	if (clearPresentationStarted_) {
		update_clear_ui();
	}

	const bool cameraFinished = !clearCameraEffectStarted_ ||
		(followCamera_ && followCamera_->is_goal_effect_finished());
	if (clearSequenceStarted_ && !confettiEffectStarted_ && cameraFinished) {
		confettiEffectStarted_ = true;
		start_confetti_effect();
	}
}

bool GamePlayScript::is_clear_camera_effect_finished() const noexcept {
	return clearCameraEffectStarted_ && followCamera_ && followCamera_->is_goal_effect_finished();
}

//============================================================================
// GamePlay.param と GoalParameter.param を読み込む。
//=============================================================================
void GamePlayScript::setup_json_asset() {

	//------------------------------------------------------------
	// GamePlay.param
	// クリア時のテキストのスライドイン位置と時間
	//------------------------------------------------------------
	szg::JsonAsset parameter{ "[[game]]/GamePlay.param" };
	const nlohmann::json& json = parameter.cget();
	if (json.is_object()) {
		const auto readR32 = [&json](const char* name, r32 fallback) {
			return json.value(name, nlohmann::json::object()).value("value", fallback);
		};
		clearTextStartX_ = readR32("ClearTextStartX", clearTextStartX_);
		clearTextTargetX_ = readR32("ClearTextTargetX", clearTextTargetX_);
		clearTextSlideDuration_ = std::max(
			readR32("ClearTextSlideDuration", clearTextSlideDuration_), 0.001f);
		cameraInitialDistance_ = std::max(
			readR32("CameraInitialDistance", cameraInitialDistance_), 0.0f);
		cameraFitPadding_ = std::max(
			readR32("CameraFitPadding", cameraFitPadding_), 1.0f);
		cameraInitialYawDegrees_ = readR32("CameraInitialYawDegrees", cameraInitialYawDegrees_);
		cameraInitialPitchDegrees_ = readR32("CameraInitialPitchDegrees", cameraInitialPitchDegrees_);
	}
	else {
		szgWarning("GamePlayScript: GamePlay.param could not be loaded. Default values are used.");
	}


	//------------------------------------------------------------
	// GoalParameter.param
	// ゴールのクリア演出のパラメータ
	//------------------------------------------------------------
	szg::JsonAsset goalParameter{ "[[game]]/GoalParameter.param" };
	const nlohmann::json& goalJson = goalParameter.cget();
	if (!goalJson.is_object()) {
		szgWarning("GamePlayScript: GoalParameter.param could not be loaded. Default values are used.");
		return;
	}
	const auto readGoalR32 = [&goalJson](const char* name, r32 fallback) {
		return goalJson.value(name, nlohmann::json::object()).value("value", fallback);
	};
	const auto readGoalVector3 = [&goalJson](const char* name, const Vector3& fallback) {
		const nlohmann::json value = goalJson.value(name, nlohmann::json::object())
			.value("value", nlohmann::json::object());
		return Vector3{
			value.value("X", fallback.x),
			value.value("Y", fallback.y),
			value.value("Z", fallback.z),
		};
	};

	goalFinalPlayerOffset_ = readGoalVector3("GoalFinalPlayerOffset", goalFinalPlayerOffset_);
	goalClearRiseHeight_ = std::max(
		readGoalR32("GoalClearRiseHeight", goalClearRiseHeight_), 0.0f);
	goalClearRiseDuration_ = std::max(
		readGoalR32("GoalClearRiseDuration", goalClearRiseDuration_), 0.001f);
	goalClearFallDuration_ = std::max(
		readGoalR32("GoalClearFallDuration", goalClearFallDuration_), 0.001f);
	clearCameraFinalOffset_ = readGoalVector3(
		"CameraFinalPlayerOffset", clearCameraFinalOffset_);
	clearCameraTargetOffset_ = readGoalVector3(
		"CameraTargetPlayerOffset", clearCameraTargetOffset_);
	clearCameraDuration_ = std::max(
		readGoalR32("CameraZoomDuration", clearCameraDuration_), 0.001f);
	clearCameraBounceStrength_ = std::max(
		readGoalR32("CameraBounceStrength", clearCameraBounceStrength_), 0.0f);
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

	for (size_t i = 0; i < kConfettiEmitterNames.size(); ++i) {
		Reference<szg::EmitterInstance>& emitter = confettiEmitters_[i];
		emitter = szg::RuntimeStorage::GetValue<Reference<szg::EmitterInstance>>(
			"RuntimeInstance", kConfettiEmitterNames[i]).value_or(nullptr);
		if (!emitter) {
			szgWarning("GamePlayScript: {} runtime instance not found.", kConfettiEmitterNames[i]);
			continue;
		}

		// シーン配置したEmitterをクリア時の一度だけの紙吹雪として使う。
		szg::EmitterInstanceSettings settings = emitter->settings_imm();
		settings.schedule.infinite = false;
		settings.schedule.cycles = 1;
		emitter->setup_settings(settings);
		emitter->set_active(false);
		if (Reference<szg::ParticlePool> pool = emitter->pool_mut()) {
			pool->clear();
		}
	}
}

//============================================================================
// ClearUICameraの座標系へシーン配置した紙吹雪を開始する。
//
void GamePlayScript::start_confetti_effect() {
	for (Reference<szg::EmitterInstance> emitter : confettiEmitters_) {
		if (!emitter) {
			continue;
		}

		// update_affine()は非Active時に更新されないため、先に有効化する。
		emitter->set_active(true);
		emitter->update_affine();
		emitter->restart_schedule();
	}
}

void GamePlayScript::stop_confetti_effect() {
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

void GamePlayScript::set_gameplay_ui_visible(bool visible) {
	for (Reference<szg::Rect3d> ui : gameplayUi_) {
		if (ui) {
			ui->set_draw(visible);
		}
	}
}

void GamePlayScript::reset_clear_sequence() {
	clearSequenceStarted_ = false;
	clearCameraEffectStarted_ = false;
	goalClearEffectStarted_ = false;
	clearPresentationStarted_ = false;
	confettiEffectStarted_ = false;
	clearTextElapsed_ = 0.0f;
	set_gameplay_ui_visible(true);
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
	stop_confetti_effect();
}
