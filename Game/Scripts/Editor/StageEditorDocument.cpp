#include "StageEditorDocument.h"

#include <algorithm>
#include <filesystem>
#include <format>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/CSV/CSVAssetBuilder.h>
#include <Engine/Assets/IAssetBuilder.h>
#include <Engine/Saver/CSVAssetSaver.h>

namespace {

constexpr i32 MIN_SIZE = 1;

/// <summary>
/// サイズを有効範囲にクランプする
/// </summary>
i32 clamp_width(i32 v) { return std::clamp(v, MIN_SIZE, StageEditorDocument::MAX_WIDTH); }
i32 clamp_height(i32 v) { return std::clamp(v, MIN_SIZE, StageEditorDocument::MAX_HEIGHT); }
i32 clamp_depth(i32 v) { return std::clamp(v, MIN_SIZE, StageEditorDocument::MAX_DEPTH); }

} // namespace

void StageEditorDocument::set_current_layer(i32 layer) {
	if (currentLayer != layer) {
		currentLayer = layer;
		if (previewSingleLayer) {
			++changeVersion;
		}
	}
}

void StageEditorDocument::set_preview_single_layer(bool enabled) {
	if (previewSingleLayer != enabled) {
		previewSingleLayer = enabled;
		++changeVersion;
	}
}

MapChipType StageEditorDocument::get(i32 x, i32 y, i32 z) const {
	return is_inside(x, y, z) ? chips[flat_index(x, y, z)] : MapChipType::Empty;
}

void StageEditorDocument::set(i32 x, i32 y, i32 z, MapChipType type) {
	if (!is_inside(x, y, z)) {
		return;
	}
	if (isEditing && !editSnapshotPushed) {
		push_undo();
		editSnapshotPushed = true;
	}
	chips[flat_index(x, y, z)] = type;
	++changeVersion;
}

void StageEditorDocument::create_new(i32 width, i32 height, i32 depth) {
	push_undo();

	sizeX = clamp_width(width);
	sizeY = clamp_height(height);
	sizeZ = clamp_depth(depth);
	chips.assign(static_cast<size_t>(sizeX * sizeY * sizeZ), MapChipType::Empty);
	currentStageNumber = MapChipField::CountStages() + 1;
	++changeVersion;
}

bool StageEditorDocument::load(i32 stageNumber) {
	const std::string directory = MapChipField::StageDirectory(stageNumber);

	std::vector<szg::CSVAsset<i32>> layers;
	for (i32 i = 1; ; ++i) {
		std::filesystem::path file = szg::IAssetBuilder::ResolveFilePath(std::format("{}/layer{:02}.csv", directory, i), "csv");
		if (!std::filesystem::exists(file)) {
			break;
		}
		std::optional<szg::CSVAsset<i32>> csv = szg::CSVAssetBuilder{}.load_from_file<i32>(file);
		if (!csv) {
			break;
		}
		layers.emplace_back(std::move(*csv));
	}

	if (layers.empty()) {
		szgWarning("StageEditorDocument: layer csv not found in '{}'", directory);
		return false;
	}

	push_undo();

	// サイズは全層・全行の最大値（不揃いは Empty 埋め）
	sizeY = static_cast<i32>(layers.size());
	sizeZ = 0;
	sizeX = 0;
	for (const auto& layer : layers) {
		sizeZ = std::max(sizeZ, static_cast<i32>(layer.size_row()));
		for (i64 row = 0; row < layer.size_row(); ++row) {
			sizeX = std::max(sizeX, static_cast<i32>(layer.size_col(row)));
		}
	}

	chips.assign(static_cast<size_t>(sizeX * sizeY * sizeZ), MapChipType::Empty);
	for (i32 y = 0; y < sizeY; ++y) {
		const auto& layer = layers[y];
		for (i32 z = 0; z < layer.size_row(); ++z) {
			for (i32 x = 0; x < layer.size_col(z); ++x) {
				chips[flat_index(x, y, z)] = static_cast<MapChipType>(layer.at(z, x));
			}
		}
	}

	currentStageNumber = stageNumber;
	++changeVersion;

	return true;
}

bool StageEditorDocument::save() {
	std::filesystem::path dir = szg::IAssetBuilder::ResolveFilePath(MapChipField::StageDirectory(currentStageNumber), "csv");

	// 既存の layer*.csv を削除（リサイズでレイヤー数が減った場合の残骸を防ぐ）
	if (std::filesystem::exists(dir)) {
		for (const auto& entry : std::filesystem::directory_iterator(dir)) {
			const std::string name = entry.path().filename().string();
			if (name.starts_with("layer") && name.ends_with(".csv")) {
				std::filesystem::remove(entry.path());
			}
		}
	}
	else {
		std::filesystem::create_directories(dir);
	}

	for (i32 y = 0; y < sizeY; ++y) {
		std::vector<std::vector<i32>> rows;
		rows.reserve(sizeZ);
		for (i32 z = 0; z < sizeZ; ++z) {
			std::vector<i32> cols;
			cols.reserve(sizeX);
			for (i32 x = 0; x < sizeX; ++x) {
				cols.emplace_back(static_cast<i32>(chips[flat_index(x, y, z)]));
			}
			rows.emplace_back(std::move(cols));
		}
		szg::CSVAsset<i32> csv{ rows };
		szg::CSVAssetSaver<i32> saver{ csv };
		saver.save_to(dir / std::format("layer{:02}.csv", y + 1));
	}

	szgInformation("StageEditorDocument: saved Stage{:02} ({}x{}x{})", currentStageNumber, sizeX, sizeY, sizeZ);
	return true;
}

void StageEditorDocument::resize(i32 width, i32 height, i32 depth) {
	const i32 newX = clamp_width(width);
	const i32 newY = clamp_height(height);
	const i32 newZ = clamp_depth(depth);

	if (newX == sizeX && newY == sizeY && newZ == sizeZ) {
		return;
	}

	push_undo();
	rebuild_chips(newX, newY, newZ);
	++changeVersion;
}

void StageEditorDocument::begin_edit() {
	isEditing = true;
	editSnapshotPushed = false;
}

void StageEditorDocument::end_edit() {
	isEditing = false;
	editSnapshotPushed = false;
}

bool StageEditorDocument::can_undo() const {
	return !undoStack.empty();
}

bool StageEditorDocument::can_redo() const {
	return !redoStack.empty();
}

void StageEditorDocument::undo() {
	if (undoStack.empty()) {
		return;
	}
	redoStack.emplace_back(StageSnapshot{ sizeX, sizeY, sizeZ, chips });
	if (redoStack.size() > MAX_HISTORY) {
		redoStack.pop_front();
	}
	apply_snapshot(undoStack.back());
	undoStack.pop_back();
	++changeVersion;
}

void StageEditorDocument::redo() {
	if (redoStack.empty()) {
		return;
	}
	undoStack.emplace_back(StageSnapshot{ sizeX, sizeY, sizeZ, chips });
	if (undoStack.size() > MAX_HISTORY) {
		undoStack.pop_front();
	}
	apply_snapshot(redoStack.back());
	redoStack.pop_back();
	++changeVersion;
}

void StageEditorDocument::push_undo() {
	undoStack.emplace_back(StageSnapshot{ sizeX, sizeY, sizeZ, chips });
	if (undoStack.size() > MAX_HISTORY) {
		undoStack.pop_front();
	}
	redoStack.clear();
}

void StageEditorDocument::apply_snapshot(const StageSnapshot& snapshot) {
	sizeX = snapshot.sizeX;
	sizeY = snapshot.sizeY;
	sizeZ = snapshot.sizeZ;
	chips = snapshot.chips;
}

void StageEditorDocument::rebuild_chips(i32 newX, i32 newY, i32 newZ) {
	std::vector<MapChipType> newChips(static_cast<size_t>(newX * newY * newZ), MapChipType::Empty);
	const i32 copyX = std::min(sizeX, newX);
	const i32 copyY = std::min(sizeY, newY);
	const i32 copyZ = std::min(sizeZ, newZ);

	for (i32 y = 0; y < copyY; ++y) {
		for (i32 z = 0; z < copyZ; ++z) {
			for (i32 x = 0; x < copyX; ++x) {
				newChips[x + newX * (z + newZ * y)] = chips[flat_index(x, y, z)];
			}
		}
	}

	sizeX = newX;
	sizeY = newY;
	sizeZ = newZ;
	chips = std::move(newChips);
}

i32 StageEditorDocument::flat_index(i32 x, i32 y, i32 z) const {
	return x + sizeX * (z + sizeZ * y);
}

bool StageEditorDocument::is_inside(i32 x, i32 y, i32 z) const {
	return 0 <= x && x < sizeX && 0 <= y && y < sizeY && 0 <= z && z < sizeZ;
}
