#pragma once

#include <array>
#include <optional>

#include <Engine/Runtime/Particle/EmitterSettings.h>
#include <Engine/Module/Render/RenderPipeline/Posteffect/Bloom/BloomPipeline.h>
#include <Engine/Runtime/Input/InputHandler.h>
#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Engine/Runtime/SceneScript/SceneScriptManager.h>
#include <Library/Utility/Template/Reference.h>

namespace szg {
class EmitterInstance;
class StringRectInstance;
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
	void create_confetti_emitters();
	void destroy_confetti_emitters();
	void start_confetti_effect();
	void start_clear_ui();
	void update_clear_ui();
	void reset_clear_sequence();

	/// 登録順が各インゲームスクリプトの更新順になる
	szg::SceneScriptManager inGameScriptManager_;

	/// 今後のインゲーム全体処理から各要素を参照するために保持する
	Reference<MapTestScript> mapTest_;
	Reference<Player> player_;
	Reference<FollowCamera> followCamera_;
	Reference<GoalManager> goalManager_;
	Reference<UndoManager> undoManager_;
	Reference<szg::WorldRoot> worldRoot_;
	Reference<szg::StringRectInstance> clearText_;
	std::array<Reference<szg::EmitterInstance>, 2> confettiEmitters_;
	std::optional<szg::EmitterInstanceSettings> confettiSettings_;
	/// RenderPath.json の Bloom ノード(EffectTag "ClayGlow")のパラメータ。接続した粘土の光の強さ
	Reference<szg::BloomPipeline::Data> clayGlow_;
	szg::InputHandler<szg::KeyID> keyInput_;
	szg::InputHandler<szg::PadID> padInput_;

	bool isSetup_{ false };
	bool sceneTransitionRequested_{ false };
	bool clearSequenceStarted_{ false };
	bool clearCameraEffectStarted_{ false };
	bool goalClearEffectStarted_{ false };
	bool clearPresentationStarted_{ false };
	/// Y を押しっぱなしで繰り返しリセットしないための発火済みフラグ
	bool resetHoldConsumed_{ false };

	r32 clearCameraDuration_{ 1.4f };
	r32 clearCameraDistance_{ 4.5f };
	r32 clearCameraElevationDegrees_{ 38.0f };
	r32 clearCameraTargetHeight_{ 0.8f };
	r32 clearCameraBounceStrength_{ 1.1f };
	r32 confettiHorizontalOffset_{ 2.5f };
	r32 confettiVerticalOffset_{ 0.8f };
	r32 clearTextStartX_{ -14.0f };
	r32 clearTextTargetX_{ 0.0f };
	r32 clearTextSlideDuration_{ 0.65f };
	r32 clearTextElapsed_{ 0.0f };
};
