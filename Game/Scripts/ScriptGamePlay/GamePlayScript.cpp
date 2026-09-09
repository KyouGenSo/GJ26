#include "GamePlayScript.h"

#include <algorithm>
#include <array>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/Json/JsonAsset.h>
#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Module/World/Mesh/Primitive/Rect3d.h>
#include <Engine/Module/World/Mesh/SkinningMeshInstance.h>
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
	setup_json_asset();
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

	inGameScriptManager_.finalize();
	mapTest_.reset();
	player_.reset();
	followCamera_.reset();
	goalManager_.reset();
	undoManager_.reset();
	clayGlow_.reset();
	resetGaugeFill_.reset();
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
		clearCameraEffectStarted_ = false;
		if (followCamera_) {
			followCamera_->stop_goal_effect();
		}
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

	if (!clearCameraEffectStarted_ && goalManager_ && goalManager_->is_cleared() &&
		followCamera_ && player_) {
		const Reference<const szg::WorldInstance> playerInstance = player_->get_world_instance_imm();
		if (playerInstance) {
			Vector3 targetPosition = playerInstance->world_position();
			targetPosition.y += clearCameraTargetHeight_;
			clearCameraEffectStarted_ = followCamera_->start_goal_effect(
				targetPosition,
				clearCameraDuration_,
				clearCameraDistance_,
				clearCameraElevationDegrees_,
				clearCameraBounceStrength_);
		}
	}

	// カメラ演出完了後もゲーム画面に留まる。クリアUIは
	// is_clear_camera_effect_finished() を使って後から表示できる。
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
}
