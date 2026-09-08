#pragma once

#include <Engine/Module/Render/RenderPipeline/Posteffect/Bloom/BloomPipeline.h>
#include <Engine/Runtime/Input/InputHandler.h>
#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Engine/Runtime/SceneScript/SceneScriptManager.h>
#include <Library/Utility/Template/Reference.h>

namespace szg {
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

	/// 登録順が各インゲームスクリプトの更新順になる
	szg::SceneScriptManager inGameScriptManager_;

	/// 今後のインゲーム全体処理から各要素を参照するために保持する
	Reference<MapTestScript> mapTest_;
	Reference<Player> player_;
	Reference<FollowCamera> followCamera_;
	Reference<GoalManager> goalManager_;
	Reference<UndoManager> undoManager_;
	/// RenderPath.json の Bloom ノード(EffectTag "ClayGlow")のパラメータ。接続した粘土の光の強さ
	Reference<szg::BloomPipeline::Data> clayGlow_;
	szg::InputHandler<szg::KeyID> keyInput_;
	szg::InputHandler<szg::PadID> padInput_;

	bool isSetup_{ false };
	bool sceneTransitionRequested_{ false };
	bool clearCameraEffectStarted_{ false };
	/// Y を押しっぱなしで繰り返しリセットしないための発火済みフラグ
	bool resetHoldConsumed_{ false };

	r32 clearCameraDuration_{ 1.4f };
	r32 clearCameraDistance_{ 4.5f };
	r32 clearCameraElevationDegrees_{ 38.0f };
	r32 clearCameraTargetHeight_{ 0.8f };
	r32 clearCameraBounceStrength_{ 1.1f };
};
