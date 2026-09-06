#pragma once

#ifdef DEBUG_FEATURES_ENABLE

#include <Engine/Debug/Editor/Window/IEditorWindow.h>

#include "Scripts/MapChip/MapChipField.h"

/// <summary>
/// 内蔵エディターの「ステージエディタ」ウィンドウ
/// </summary>
class StageEditorWindow final : public szg::IEditorWindow {
public:
	StageEditorWindow();
	~StageEditorWindow() = default;

	StageEditorWindow(const StageEditorWindow&) = delete;
	StageEditorWindow& operator=(const StageEditorWindow&) = delete;
	StageEditorWindow(StageEditorWindow&&) = default;
	StageEditorWindow& operator=(StageEditorWindow&&) = default;

public:
	void draw() override;

private:
	void draw_stage_operations();
	void draw_new_stage();
	void draw_resize();
	void draw_chip_select();
	void draw_layer_select();
	void draw_grid();
	void draw_preview_note();
	void draw_undo_redo();

private:
	static int chip_to_int(MapChipType type);
	static MapChipType int_to_chip(int value);

private:
	MapChipType selectedChip{ MapChipType::Clay };
	u8 selectedFaces{ ClayFace::None }; // 粘土を塗るときに付ける伸ばせない面

	i32 newWidth{ 8 };
	i32 newHeight{ 2 };
	i32 newDepth{ 8 };

	i32 resizeWidth{ 8 };
	i32 resizeHeight{ 2 };
	i32 resizeDepth{ 8 };

	i32 selectedStageIndex{ 0 };
	i32 loadStageNumber{ 1 };

	bool isPainting{ false };
};

#endif // DEBUG_FEATURES_ENABLE
