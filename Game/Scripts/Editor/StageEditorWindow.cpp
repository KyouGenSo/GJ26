#include "StageEditorWindow.h"

#ifdef DEBUG_FEATURES_ENABLE

#include <algorithm>
#include <cstdio>
#include <format>

#include <imgui.h>

#include <Engine/Application/Logger.h>

#include "StageEditorDocument.h"

namespace {

/// <summary>
/// チップ種類を日本語ラベルで表示
/// </summary>
const char* ChipLabel(MapChipType type) {
	switch (type) {
	case MapChipType::Empty:
		return "消去";
	case MapChipType::Clay:
		return "粘土";
	case MapChipType::GoalPiece:
		return "ゴール";
	default:
		return "?";
	}
}

/// <summary>
/// ImGui で使う色に変換（Empty は無効っぽい濃いグレー）
/// </summary>
ImVec4 ChipColor(MapChipType type) {
	switch (type) {
	case MapChipType::Clay:
		return ImVec4{ 0.55f, 0.35f, 0.20f, 1.0f };
	case MapChipType::GoalPiece:
		return ImVec4{ 1.0f, 1.0f, 0.0f, 1.0f };
	default:
		return ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f };
	}
}

} // namespace

StageEditorWindow::StageEditorWindow() {
	StageEditorDocument& doc = StageEditorDocument::GetInstance();
	if (doc.width() == 0) {
		// 初回：Stage01 を読み込んでおく
		doc.load(1);
	}
	loadStageNumber = doc.stage_number();
	selectedStageIndex = loadStageNumber - 1;
	resizeWidth = doc.width() > 0 ? doc.width() : newWidth;
	resizeHeight = doc.height() > 0 ? doc.height() : newHeight;
	resizeDepth = doc.depth() > 0 ? doc.depth() : newDepth;
	newWidth = resizeWidth;
	newHeight = resizeHeight;
	newDepth = resizeDepth;
}

void StageEditorWindow::draw() {
	update_focus();

	StageEditorDocument& doc = StageEditorDocument::GetInstance();

	// ドキュメントサイズに合わせてレイヤー番号をクランプ
	if (doc.height() > 0) {
		doc.set_current_layer(std::clamp(doc.current_layer(), 1, doc.height()));
	}
	else {
		doc.set_current_layer(1);
	}

	// Undo/Redo ショートカット
	if (is_focus()) {
		if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
			doc.undo();
		}
		if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
			doc.redo();
		}
	}

	// マウスボタンが離れたら編集終了
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		if (isPainting) {
			doc.end_edit();
			isPainting = false;
		}
	}

	draw_undo_redo();
	ImGui::Separator();
	draw_stage_operations();
	ImGui::Separator();
	draw_new_stage();
	ImGui::Separator();
	draw_resize();
	ImGui::Separator();
	draw_chip_select();
	ImGui::Separator();
	draw_layer_select();
	ImGui::Separator();
	draw_grid();
	ImGui::Separator();
	draw_preview_note();
}

void StageEditorWindow::draw_undo_redo() {
	StageEditorDocument& doc = StageEditorDocument::GetInstance();

	if (ImGui::Button("元に戻す")) {
		doc.undo();
	}
	ImGui::SameLine();
	if (ImGui::Button("やり直し")) {
		doc.redo();
	}
}

void StageEditorWindow::draw_stage_operations() {
	StageEditorDocument& doc = StageEditorDocument::GetInstance();

	ImGui::Text("ステージ操作");

	// ステージ一覧
	const i32 stageCount = MapChipField::CountStages();
	std::vector<std::string> stageNames;
	stageNames.reserve(stageCount);
	for (i32 i = 1; i <= stageCount; ++i) {
		stageNames.emplace_back(std::format("Stage{:02}", i));
	}

	if (selectedStageIndex >= stageCount) {
		selectedStageIndex = std::max(0, stageCount - 1);
	}

	if (stageCount > 0) {
		std::vector<const char*> items;
		items.reserve(stageNames.size());
		for (const auto& name : stageNames) {
			items.emplace_back(name.c_str());
		}
		if (ImGui::ListBox("ステージ一覧", &selectedStageIndex, items.data(), static_cast<i32>(items.size()), 4)) {
			loadStageNumber = selectedStageIndex + 1;
		}
	}
	else {
		ImGui::Text("ステージがありません");
	}

	// ステージ番号
	if (ImGui::InputInt("ステージ番号", &loadStageNumber, 1, 100)) {
		loadStageNumber = std::max(1, loadStageNumber);
		if (loadStageNumber <= stageCount) {
			selectedStageIndex = loadStageNumber - 1;
		}
	}

	if (ImGui::Button("読み込み")) {
		doc.load(loadStageNumber);
		resizeWidth = doc.width();
		resizeHeight = doc.height();
		resizeDepth = doc.depth();
	}
	ImGui::SameLine();
	if (ImGui::Button("保存")) {
		doc.save();
	}
}

void StageEditorWindow::draw_new_stage() {
	StageEditorDocument& doc = StageEditorDocument::GetInstance();

	ImGui::Text("新規作成");
	ImGui::InputInt("幅(X)##new", &newWidth, 1, 1);
	ImGui::InputInt("高さ(Y)##new", &newHeight, 1, 1);
	ImGui::InputInt("奥行(Z)##new", &newDepth, 1, 1);

	newWidth = std::clamp(newWidth, 1, StageEditorDocument::MAX_WIDTH);
	newHeight = std::clamp(newHeight, 1, StageEditorDocument::MAX_HEIGHT);
	newDepth = std::clamp(newDepth, 1, StageEditorDocument::MAX_DEPTH);

	if (ImGui::Button("新規作成")) {
		doc.create_new(newWidth, newHeight, newDepth);
		resizeWidth = doc.width();
		resizeHeight = doc.height();
		resizeDepth = doc.depth();
		doc.set_current_layer(1);
	}
}

void StageEditorWindow::draw_resize() {
	StageEditorDocument& doc = StageEditorDocument::GetInstance();

	ImGui::Text("リサイズ");
	ImGui::InputInt("幅(X)##resize", &resizeWidth, 1, 1);
	ImGui::InputInt("高さ(Y)##resize", &resizeHeight, 1, 1);
	ImGui::InputInt("奥行(Z)##resize", &resizeDepth, 1, 1);

	resizeWidth = std::clamp(resizeWidth, 1, StageEditorDocument::MAX_WIDTH);
	resizeHeight = std::clamp(resizeHeight, 1, StageEditorDocument::MAX_HEIGHT);
	resizeDepth = std::clamp(resizeDepth, 1, StageEditorDocument::MAX_DEPTH);

	if (ImGui::Button("リサイズ適用")) {
		doc.resize(resizeWidth, resizeHeight, resizeDepth);
	}
}

void StageEditorWindow::draw_chip_select() {
	ImGui::Text("チップ選択");

	int selected = chip_to_int(selectedChip);
	bool changed = false;

	changed |= ImGui::RadioButton("消去 (0)", &selected, chip_to_int(MapChipType::Empty));
	changed |= ImGui::RadioButton("粘土 (1)", &selected, chip_to_int(MapChipType::Clay));
	changed |= ImGui::RadioButton("ゴール (2)", &selected, chip_to_int(MapChipType::GoalPiece));

	if (changed) {
		selectedChip = int_to_chip(selected);
	}
}

void StageEditorWindow::draw_layer_select() {
	StageEditorDocument& doc = StageEditorDocument::GetInstance();

	ImGui::Text("レイヤー選択");
	const i32 height = std::max(1, doc.height());
	ImGui::Text("現在のレイヤー: %d / %d", doc.current_layer(), height);
	i32 currentLayer = doc.current_layer();
	if (ImGui::SliderInt("レイヤー", &currentLayer, 1, height)) {
		doc.set_current_layer(currentLayer);
	}
	if (ImGui::Button("前のレイヤー") && doc.current_layer() > 1) {
		doc.set_current_layer(doc.current_layer() - 1);
	}
	ImGui::SameLine();
	if (ImGui::Button("次のレイヤー") && doc.current_layer() < height) {
		doc.set_current_layer(doc.current_layer() + 1);
	}

	bool previewSingleLayer = doc.preview_single_layer();
	if (ImGui::Checkbox("1レイヤーのみプレビュー", &previewSingleLayer)) {
		doc.set_preview_single_layer(previewSingleLayer);
	}
}

void StageEditorWindow::draw_grid() {
	StageEditorDocument& doc = StageEditorDocument::GetInstance();

	if (doc.width() <= 0 || doc.depth() <= 0 || doc.height() <= 0) {
		ImGui::Text("ステージが読み込まれていません");
		return;
	}

	ImGui::Text("レイヤー編集（左クリック：設置 / 右クリック：消去 / ドラッグ：連続）");

	const i32 y = doc.current_layer() - 1;
	const float cellSize = 28.0f;

	const ImVec2 buttonSize{ cellSize, cellSize };

	for (i32 z = 0; z < doc.depth(); ++z) {
		for (i32 x = 0; x < doc.width(); ++x) {
			ImGui::PushID(static_cast<i32>(x + z * doc.width()));

			MapChipType chip = doc.get(x, y, z);
			ImVec4 color = ChipColor(chip);
			ImGui::PushStyleColor(ImGuiCol_Button, color);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);

			char label[8];
			snprintf(label, sizeof(label), "%d", static_cast<i32>(chip));
			ImGui::Button(label, buttonSize);

			ImGui::PopStyleColor(3);

			if (ImGui::IsItemHovered()) {
				if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					if (!isPainting) {
						doc.begin_edit();
						isPainting = true;
					}
					doc.set(x, y, z, selectedChip);
				}
				else if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
					if (!isPainting) {
						doc.begin_edit();
						isPainting = true;
					}
					doc.set(x, y, z, MapChipType::Empty);
				}
			}

			ImGui::PopID();

			if (x + 1 < doc.width()) {
				ImGui::SameLine();
			}
		}
	}
}

void StageEditorWindow::draw_preview_note() {
	ImGui::Text("プレビュー説明");
	ImGui::Text("3Dプレビューは内蔵エディターのSceneビューで確認する");
	ImGui::Text("Sceneビューでは上下左右360度の視点操作が可能");
	ImGui::Text("F5でテストシーンへ移動できる");
}

int StageEditorWindow::chip_to_int(MapChipType type) {
	return static_cast<int>(type);
}

MapChipType StageEditorWindow::int_to_chip(int value) {
	return static_cast<MapChipType>(value);
}

#endif // DEBUG_FEATURES_ENABLE
