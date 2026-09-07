#pragma once

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

private:
	/// 登録順が各インゲームスクリプトの更新順になる
	szg::SceneScriptManager inGameScriptManager_;

	/// 今後のインゲーム全体処理から各要素を参照するために保持する
	Reference<MapTestScript> mapTest_;
	Reference<Player> player_;
	Reference<FollowCamera> followCamera_;
	Reference<GoalManager> goalManager_;
	szg::InputHandler<szg::KeyID> keyInput_;
	szg::InputHandler<szg::PadID> padInput_;

	bool isSetup_{ false };
	bool sceneTransitionRequested_{ false };
};
