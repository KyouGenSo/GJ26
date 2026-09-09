#include "TitleScene.h"
#include <array>
#include <cmath>
#include <numbers>
#include <optional>

#include "Scripts/Common/LoadingTransitionController.h"
#include "Scripts/Instance/FollowCamera/FollowCamera.h"
#include "Scripts/Instance/Skydome/Skydome.h"
#include "Scripts/Manager/SoundPlayer.h"
#include "Scripts/Scene/FactoryGJ26.h"
#include <Engine/Application/Logger.h>
#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Module/World/Mesh/Primitive/Rect3d.h>
#include <Engine/Runtime/Clock/WorldClock.h>
#include <Engine/Runtime/Input/InputHandler.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Runtime/Scene/SceneManager2.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>
#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Library/Utility/Tools/SmartPointer.h>

namespace {

/// A ボタン UI が上下に 1 往復する秒数
constexpr r32 kStartButtonFloatPeriodSeconds = 2.0f;
/// A ボタン UI が基準位置から上下に動く最大距離
constexpr r32 kStartButtonFloatAmplitude = 0.1f;
/// 寝息を鳴らし直す間隔(秒)。
constexpr r32 kSleepingBreathIntervalSeconds = 1.5f;
/// タイトルで使う音。BGM はループ、寝息はタイマーで鳴らし直し、決定音はシーン遷移をまたいで鳴らす
constexpr std::array<string_literal, 3> kSounds{ "titleBgm.wav", "sleepingBreath.wav", "decision.wav" };

/// シーン開始時のフェードインにかける時間[秒]
constexpr r32 kFadeInDurationSeconds = 0.8f;
/// フェードイン中に A ボタン押下をスキップ可能とみなす最低進捗[0, 1]
/// (例: 0.2 ならフェードインが 20% 以上進んだ時点で現在のアルファから即フェードアウト開始を許可)
constexpr r32 kFadeInSkipProgressThreshold = 0.2f;
/// 決定後のフェードアウトにかける時間[秒]
constexpr r32 kFadeOutDurationSeconds = 0.5f;
/// SceneChange の interval 引数に渡す、フェードアウト＋BGロードのための最大待ち時間[秒]
constexpr r32 kSceneChangeIntervalSeconds = 5.0f;
/// UI カメラ(直交投影)の表示範囲。フェード用 Rect3d のサイズをこれに合わせて画面外にはみ出させる
constexpr Vector2 kFadeOverlaySize{ 19.2f, 10.8f };
/// フェード用 Rect3d を置く Z 座標。UI カメラの可視範囲 [0, 5] 内で StartButton(Z=1) より手前に置く
constexpr r32 kFadeOverlayDepth = 3.0f;
/// "NOW LOADING" テキストの接地点 Z 座標。フェード(Z=2.0)より手前かつ FarClip(Z=5)以内に置く
constexpr r32 kLoadingTextDepth = 2.0f;
/// UI カメラスケールに合わせたロード演出テキストの文字サイズ
constexpr r32 kLoadingTextFontSize = 1.0f;

/// <summary>
/// タイトルの開始入力と A ボタン UI の浮遊演出、およびフェードイン/アウト演出
/// </summary>
class TitleScript final : public szg::ISceneScript {
public:
	TitleScript() {
		pad_.initialize({ szg::PadID::A }, szg::InputInitializeMode::Current);
		mouse_.initialize({ szg::MouseID::Left }, szg::InputInitializeMode::Current);
		sound_.initialize(kSounds);
		sound_.play("titleBgm.wav");
		sound_.play("sleepingBreath.wav");
	}
	~TitleScript() override = default;

	SZG_CLASS_MOVE_ONLY(TitleScript)

public:
	/// UI World と StartButton を受け取り、フェード演出を構築してシーン開始時のフェードインを開始する
	void setup(Reference<szg::WorldCluster> uiWorld, Reference<szg::Rect3d> startButton) {
		startButton_ = startButton;
		if (startButton_) {
			startButtonBasePosition_ = startButton_->transform_imm().get_translate();
		}

		if (uiWorld) {
			LoadingTransitionController::Config config{};
			config.fadeSize = kFadeOverlaySize;
			config.fadeLayer = 1;     // StartButton(レイヤー 0)より手前
			config.fadeDepth = kFadeOverlayDepth;
			config.fadeDuration = kFadeOutDurationSeconds;
			config.fadeColor = ColorRGB(0.0f, 0.0f, 0.0f);

			config.loadingText = "NOW LOADING";
			config.textBasePosition = Vector3{ 0.0f, 0.0f, kLoadingTextDepth };
			config.textFontSize = kLoadingTextFontSize;
			config.textLayer = 1;      // フェードと同じレイヤー。Z ソートでフェード(Z=2.0)の奥にテキスト(Z=2.95)が来るので、暗転中もテキストは見える

			loadingTransition_.Create(uiWorld, config);
			// シーン開始時は暗転状態からフェードイン。StartFadeIn 完了まで A 入力は無視される
			loadingTransition_.StartFadeIn(kFadeInDurationSeconds);
		}
		else {
			szgWarning("Title: UI world not provided. Loading transition is disabled.");
		}
	}

	void prev_update() override {
		const r32 deltaSeconds = szg::WorldClock::DeltaSeconds();
		update_start_button_float();
		update_sleeping_breath();

		// LoadingTransitionController を駆動する。フェードイン/アウト両方を Update 1 つで進行させる
		loadingTransition_.Update(deltaSeconds);

		pad_.update();
		mouse_.update();

		// フェードアウトが終わり BG ロードも完了した場合はシーン遷移を実行する
		if (transitionRequested_ && loadingTransition_.IsReadyToProceed()) {
			szg::SceneManager2::EndSceneChangeIntervalForce();
			transitionRequested_ = false;
			return;
		}

		// フェードイン中は進捗が閾値未満なら決定入力を受け付けない(序盤で誤遷移するのを防止)。
		// 閾値以上なら A 入力受付にそのまま流し、Begin() が現在のアルファ値から暗転復帰を開始する
		if (loadingTransition_.IsFadingIn() && loadingTransition_.FadeInProgress() < kFadeInSkipProgressThreshold) {
			return;
		}

		if (!transitionRequested_ && pad_.trigger(szg::PadID::A)) {
			transitionRequested_ = true;
			SoundPlayer::PlayAcrossScene("decision.wav");
			loadingTransition_.Begin();
			// フェードアウトの interval 中に BG ロードを進める(isStopLoad=false)。interval は
			// フェードアウトより十分長く取り、IsReadyToProceed() 成立時に強制遷移させる
			szg::SceneManager2::SceneChange(SceneListGJ26::Select, kSceneChangeIntervalSeconds, false, false);
		}
	}

private:
	/// 基準位置を中心に sin で上下させる
	void update_start_button_float() {
		if (!startButton_) {
			return;
		}
		floatTime_ += szg::WorldClock::DeltaSeconds();
		const r32 phase = floatTime_ * (2.0f * std::numbers::pi_v<r32> / kStartButtonFloatPeriodSeconds);
		Vector3 position = startButtonBasePosition_;
		position.y += std::sin(phase) * kStartButtonFloatAmplitude;
		startButton_->transform_mut().set_translate(position);
	}

	/// 一定間隔ごとに寝息を頭から鳴らし直す
	void update_sleeping_breath() {
		breathTime_ += szg::WorldClock::DeltaSeconds();
		if (breathTime_ < kSleepingBreathIntervalSeconds) {
			return;
		}
		breathTime_ -= kSleepingBreathIntervalSeconds;
		sound_.restart("sleepingBreath.wav");
	}

	szg::InputHandler<szg::PadID> pad_;
	szg::InputHandler<szg::MouseID> mouse_;
	bool transitionRequested_{ false };
	SoundPlayer sound_;

	Reference<szg::Rect3d> startButton_;
	Vector3 startButtonBasePosition_{ CVector3::ZERO };
	r32 floatTime_{ 0.0f };
	r32 breathTime_{ 0.0f };

	LoadingTransitionController loadingTransition_;
};

} // namespace

TitleScene::TitleScene() noexcept {
	set_name("Title");
}

void TitleScene::custom_load_asset() {
	SoundPlayer::RegisterLoadQue(kSounds);
}

void TitleScene::custom_setup() {
	if (Reference<szg::WorldCluster> world = world_mut(0)) {
		world->world_root_mut().instantiate<Skydome>(nullptr);
	}
	else {
		szgError("Title: world 0 not found.");
	}

	auto cameraInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::CameraInstance>>("RuntimeInstance", "MainCamera");
	auto cameraFollowTargetInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::WorldInstance>>("RuntimeInstance", "CameraFollowTarget");

	const auto startButtonInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::Rect3d>>("RuntimeInstance", "StartButton");
	if (!startButtonInstance) {
		szgWarning("Title: StartButton runtime instance not found.");
	}

	if (cameraInstance && cameraFollowTargetInstance) {
		followCameraScript = eps::CreateUnique<FollowCamera>(cameraInstance.value_or(nullptr), cameraFollowTargetInstance.value_or(nullptr));
	}

	if (followCameraScript) {
		sceneScriptManager.register_script(std::move(followCameraScript));
	}

	std::unique_ptr<TitleScript> titleScript = eps::CreateUnique<TitleScript>();
	titleScript->setup(world_mut(1), startButtonInstance.value_or(nullptr));
	sceneScriptManager.register_script(std::move(titleScript));
}