#pragma once

#include <deque>
#include <vector>

#include <Library/Utility/Template/SingletonInterface.h>

#include "Scripts/MapChip/MapChipField.h"

/// <summary>
/// Undo/Redo 用スナップショット
/// </summary>
struct StageSnapshot {
	i32 sizeX{ 0 };
	i32 sizeY{ 0 };
	i32 sizeZ{ 0 };
	std::vector<MapChipType> chips;
	std::vector<u8> faces;
};

/// <summary>
/// ステージエディター用のメモリ上データ管理・Undo/Redo・CSV入出力
/// </summary>
class StageEditorDocument final : public SingletonInterface<StageEditorDocument> {
	SZG_CLASS_SINGLETON(StageEditorDocument)

public:
	/// <summary>
	/// 編集対象のステージ番号を取得
	/// </summary>
	i32 stage_number() const noexcept { return currentStageNumber; }

	/// <summary>
	/// ステージサイズ
	/// </summary>
	i32 width() const noexcept { return sizeX; }
	i32 height() const noexcept { return sizeY; }
	i32 depth() const noexcept { return sizeZ; }

	/// <summary>
	/// データ変更バージョン。外部はこれを監視して再構築する。
	/// </summary>
	u32 version() const noexcept { return changeVersion; }

	/// <summary>
	/// 編集中のレイヤー番号（表示上は1始まり）
	/// </summary>
	i32 current_layer() const noexcept { return currentLayer; }
	void set_current_layer(i32 layer);

	/// <summary>
	/// プレビューで1レイヤーのみ描画するか
	/// </summary>
	bool preview_single_layer() const noexcept { return previewSingleLayer; }
	void set_preview_single_layer(bool enabled);

	/// <summary>
	/// 指定座標のチップ値を取得（範囲外は Empty）
	/// </summary>
	MapChipType get(i32 x, i32 y, i32 z) const;

	/// <summary>
	/// 指定座標のチップ値と粘土の伸ばせない面を設定（粘土以外は面を None に強制、範囲外は無視）
	/// </summary>
	void set(i32 x, i32 y, i32 z, MapChipType type, u8 blockedFaces = ClayFace::None);

	/// <summary>
	/// 指定座標の粘土の伸ばせない面（ClayFace のビット、範囲外は None）
	/// </summary>
	u8 blocked_faces(i32 x, i32 y, i32 z) const;

	/// <summary>
	/// ステージを新規作成する（全セル Empty）
	/// </summary>
	void create_new(i32 width, i32 height, i32 depth);

	/// <summary>
	/// 既存のステージディレクトリから読み込む
	/// </summary>
	/// <param name="stageNumber">1始まりのステージ番号</param>
	/// <returns>成功したら true</returns>
	bool load(i32 stageNumber);

	/// <summary>
	/// 現在のステージ番号で保存する
	/// </summary>
	/// <returns>成功したら true</returns>
	bool save();

	/// <summary>
	/// ステージサイズを変更する。既存セルは可能な範囲で維持。
	/// </summary>
	void resize(i32 width, i32 height, i32 depth);

	/// <summary>
	/// 編集操作の開始（ドラッグ1ストロークの Undo 単位を開始）
	/// </summary>
	void begin_edit();

	/// <summary>
	/// 編集操作の終了
	/// </summary>
	void end_edit();

	/// <summary>
	/// Undo / Redo
	/// </summary>
	bool can_undo() const;
	bool can_redo() const;
	void undo();
	void redo();

public:
	/// <summary>
	/// サイズ上限
	/// </summary>
	static constexpr i32 MAX_WIDTH = 32;
	static constexpr i32 MAX_HEIGHT = 16;
	static constexpr i32 MAX_DEPTH = 32;

private:
	void push_undo();
	void apply_snapshot(const struct StageSnapshot& snapshot);
	void rebuild_chips(i32 newX, i32 newY, i32 newZ);
	i32 flat_index(i32 x, i32 y, i32 z) const;
	bool is_inside(i32 x, i32 y, i32 z) const;

private:
	i32 sizeX{ 0 };
	i32 sizeY{ 0 };
	i32 sizeZ{ 0 };
	std::vector<MapChipType> chips;
	std::vector<u8> faces; // chips と同じ添字。粘土セルの伸ばせない面（ClayFace のビット）
	i32 currentStageNumber{ 1 };
	u32 changeVersion{ 0 };

	i32 currentLayer{ 1 };
	bool previewSingleLayer{ false };

	bool isEditing{ false };
	bool editSnapshotPushed{ false };

	static constexpr size_t MAX_HISTORY = 1024;
	std::deque<StageSnapshot> undoStack;
	std::deque<StageSnapshot> redoStack;
};
