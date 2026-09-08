#pragma once

#include <deque>

#include <Engine/Runtime/Input/InputHandler.h>
#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Library/Math/Vector3.h>
#include <Library/Utility/Template/Reference.h>

#include "Scripts/MapChip/MapChipField.h"

class Player;
class SoundPlayer;

/// <summary>
/// <para>ブロック操作(粘土を伸ばす / 接続 / ピースを押す・引く)の Undo</para>
/// <para>MapChipField::version() が変わったフレームを 1 手とし、その操作前のセルデータとプレイヤーの位置・向きを積む</para>
/// </summary>
class UndoManager final : public szg::ISceneScript {
public:
	UndoManager() = default;
	~UndoManager() = default;

	SZG_CLASS_MOVE_ONLY(UndoManager)

public:
	void setup(Reference<MapChipField> field_, Reference<Player> player_);

	/// <summary>
	/// 1 手戻せたときに鳴らす SE(無ければ無音)
	/// </summary>
	void set_sound(Reference<SoundPlayer> sound_);

	/// <summary>
	/// 入力で undo した後、このフレームの操作前の状態を控える
	/// </summary>
	void prev_update() override;

	/// <summary>
	/// このフレームでフィールドが変わっていれば控えた状態を履歴に積む
	/// </summary>
	void post_update() override;

	/// <summary>
	/// 履歴を捨てる(ステージ再ロード時)
	/// </summary>
	void clear();

	bool can_undo() const { return !history.empty(); }

	/// <summary>
	/// 1 手戻す。履歴が空なら何もしない
	/// </summary>
	void undo();

private:
	struct Snapshot {
		MapChipField::Cells cells;
		Vector3 playerPosition{ 0.0f, 0.0f, 0.0f };
		Vector3 playerDirection{ 0.0f, 0.0f, 1.0f };
	};

	Snapshot capture() const;
	void apply(const Snapshot& snapshot);

private:
	Reference<MapChipField> field;
	Reference<Player> player;
	Reference<SoundPlayer> sound;
	szg::InputHandler<szg::KeyID> keys;
	szg::InputHandler<szg::PadID> pad;
	bool triggerLPressed{ false }; // LT を押した瞬間を取るための前フレームの状態
	Snapshot pending; // このフレームの操作前の状態
	std::deque<Snapshot> history;
	u32 lastVersion{ 0 };
};
