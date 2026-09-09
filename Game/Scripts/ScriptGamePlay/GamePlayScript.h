#pragma once

#include <array>

#include <Engine/Module/Render/RenderPipeline/Posteffect/Bloom/BloomPipeline.h>
#include <Engine/Runtime/Input/InputHandler.h>
#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Engine/Runtime/SceneScript/SceneScriptManager.h>
#include <Library/Math/Vector3.h>
#include <Library/Utility/Template/Reference.h>

#include "Scripts/MapChip/MapChipField.h"
#include "Scripts/Manager/SoundPlayer.h"

namespace szg {
class EmitterInstance;
class StringRectInstance;
class Rect3d;
class WorldRoot;
}

class FollowCamera;
class GoalManager;
class MapTestScript;
class Player;
class UndoManager;

/// <summary>
/// インゲームを構成する各スクリプトの生成と更新順を管理する
/// </summary>
class GamePlayScript final : public szg::ISceneScript {
public:
	GamePlayScript() = default;
	~GamePlayScript() override = default;

	SZG_CLASS_MOVE_ONLY(GamePlayScript)

public:
	/// <summary>
	/// GamePlayScene::custom_load_asset で呼ぶ。インゲームの BGM / SE を登録する
	/// </summary>
	static void RegisterAudioAssets();

	/// <summary>
	/// マップ、Player、追従カメラ、ゴール判定をセットアップする
	/// </summary>
	void setup(Reference<szg::WorldRoot> worldRoot);

	void finalize() override;
	void prev_update() override;
	void post_update() override;

	/// クリアカメラ演出が最終位置へ到着済みか（クリアUI表示開始の判定用）
	bool is_clear_camera_effect_finished() const noexcept;

private:
	void setup_json_asset();
	void setup_clear_presentation();
	void start_confetti_effect();
	void stop_confetti_effect();
	void start_clear_ui();
	void update_clear_ui();
	void set_gameplay_ui_visible(bool visible);
	void reset_clear_sequence();

	/// BGM / SE。Player と UndoManager が参照するので inGameScriptManager_ より先に宣言して後に破棄する
	SoundPlayer sound_;

	/// 登録順が各インゲームスクリプトの更新順になる
	szg::SceneScriptManager inGameScriptManager_;

	/// 今後のインゲーム全体処理から各要素を参照するために保持する
	Reference<MapTestScript> mapTest_;
	Reference<Player> player_;
	Reference<FollowCamera> followCamera_;
	Reference<GoalManager> goalManager_;
	Reference<UndoManager> undoManager_;
	Reference<szg::StringRectInstance> clearText_;
	std::array<Reference<szg::Rect3d>, 5> gameplayUi_;
	std::array<Reference<szg::EmitterInstance>, 2> confettiEmitters_;
	/// RenderPath.json の Bloom ノード(EffectTag "ClayGlow")のパラメータ。接続した粘土の光の強さ
	Reference<szg::BloomPipeline::Data> clayGlow_;
	/// リセットゲージの塗り(UI.json "ResetGaugeFill")。Y 長押し時間に応じて左から伸ばす
	Reference<szg::Rect3d> resetGaugeFill_;
	/// 掴める対象の輪郭の見た目(GripHighlight.param)
	MapChipField::HighlightStyle gripHighlight_;
	szg::InputHandler<szg::KeyID> keyInput_;
	szg::InputHandler<szg::PadID> padInput_;

	bool isSetup_{ false };
	bool sceneTransitionRequested_{ false };
	bool clearSequenceStarted_{ false };
	bool clearCameraEffectStarted_{ false };
	bool goalClearEffectStarted_{ false };
	bool clearPresentationStarted_{ false };
	bool confettiEffectStarted_{ false };
	/// Y を押しっぱなしで繰り返しリセットしないための発火済みフラグ
	bool resetHoldConsumed_{ false };
	/// ゴール出現 / クリアの立ち上がりで SE を鳴らすための前フレームの状態
	bool wasGoalOpen_{ false };
	bool wasCleared_{ false };

	Vector3 goalFinalPlayerOffset_{ 0.0f, 1.8f, 0.0f };
	r32 goalClearRiseHeight_{ 3.0f };
	r32 goalClearRiseDuration_{ 0.55f };
	r32 goalClearFallDuration_{ 0.75f };
	Vector3 clearCameraFinalOffset_{ 0.0f, 3.57f, -3.55f };
	Vector3 clearCameraTargetOffset_{ 0.0f, 0.8f, 0.0f };
	r32 clearCameraDuration_{ 1.4f };
	r32 clearCameraBounceStrength_{ 1.1f };
	/// 0以下ならステージサイズに合わせてカメラ距離を自動調整する
	r32 cameraInitialDistance_{ 0.0f };
	r32 cameraFitPadding_{ 1.1f };
	r32 cameraInitialYawDegrees_{ 0.0f };
	r32 cameraInitialPitchDegrees_{ 17.188734f };
	/// ゲージ満タン時の横幅(UI.json の Size.X を setup で控える)
	r32 resetGaugeFullWidth_{ 0.0f };
	r32 clearTextStartX_{ -14.0f };
	r32 clearTextTargetX_{ 0.0f };
	r32 clearTextSlideDuration_{ 0.65f };
	r32 clearTextElapsed_{ 0.0f };
};
