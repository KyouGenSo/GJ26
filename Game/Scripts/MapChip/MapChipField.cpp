#include "MapChipField.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/CSV/CSVAssetBuilder.h>
#include <Engine/Assets/IAssetBuilder.h>
#include <Library/Math/ColorRGB.h>

namespace {

/// <summary>
/// チップ種類ごとの表示色(attached はゴール条件オブジェクトにつながった粘土)
/// </summary>
ColorRGB ChipColor(MapChipType type, bool attached) {
	switch (type) {
	case MapChipType::Clay:
		// ライトで白く飛ぶので、つながった粘土は暗い赤茶にして差を出す
		return attached ? ColorRGB{ 0.30f, 0.05f, 0.00f } : ColorRGB{ 0.55f, 0.35f, 0.20f };
	case MapChipType::GoalPiece:
		return CColorRGB::RED;
	case MapChipType::Goal:
		return CColorRGB::GREEN;
	default:
		return CColorRGB::WHITE;
	}
}

/// <summary>
/// "[[game]]/..." 形式の CSV パスが実在するか
/// </summary>
bool ExistsCsv(const std::string& path) {
	return std::filesystem::exists(szg::IAssetBuilder::ResolveFilePath(path, "csv"));
}

} // namespace

bool MapChipField::load_stage(i32 stageNumber) {
	return load(StageDirectory(stageNumber));
}

std::string MapChipField::StageDirectory(i32 stageNumber) {
	return std::format("[[game]]/Map/Stage{:02}", stageNumber);
}

i32 MapChipField::CountStages() {
	i32 count = 0;
	while (ExistsCsv(std::format("{}/layer01.csv", StageDirectory(count + 1)))) {
		++count;
	}
	return count;
}

bool MapChipField::load(const std::string& directory) {
	for (auto& visual : visuals) {
		if (visual) {
			visual->destroy_self();
		}
	}
	visuals.clear();
	++revision;

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
		szgWarning("MapChipField: layer csv not found in \'{}\'", directory);
		sizeX = sizeY = sizeZ = 0;
		chips.clear();
		clayOrigin.clear();
		clayPiece.clear();
		return false;
	}

	// サイズは全層・全行の最大値(不揃いは Empty 埋め)
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

	// CSV の粘土はそれぞれ独立した未接続のブロック
	clayOrigin.assign(chips.size(), -1);
	clayPiece.assign(chips.size(), -1);
	for (size_t i = 0; i < chips.size(); ++i) {
		if (chips[i] == MapChipType::Clay) {
			clayOrigin[i] = static_cast<i32>(i);
		}
	}
	return true;
}

void MapChipField::build(szg::WorldRoot& worldRoot_) {
	worldRoot = worldRoot_;
	visuals.assign(chips.size(), Reference<szg::StaticMeshInstance>{});

	for (i32 i = 0; i < static_cast<i32>(chips.size()); ++i) {
		refresh_visual(i);
	}
}

MapChipType MapChipField::get(i32 x, i32 y, i32 z) const {
	return is_inside(x, y, z) ? chips[flat_index(x, y, z)] : MapChipType::Empty;
}

void MapChipField::set(i32 x, i32 y, i32 z, MapChipType type) {
	if (!is_inside(x, y, z)) {
		return;
	}
	const i32 i = flat_index(x, y, z);
	chips[i] = type;
	clayOrigin[i] = type == MapChipType::Clay ? i : -1;
	clayPiece[i] = -1;
	++revision;
	refresh_visual(i);
}

bool MapChipField::stretch_clay(const MapChipIndex& from, const MapChipIndex& to) {
	if (!is_inside(from.x, from.y, from.z) || !is_inside(to.x, to.y, to.z)) {
		return false;
	}
	if (std::abs(to.x - from.x) + std::abs(to.y - from.y) + std::abs(to.z - from.z) != 1) {
		return false;
	}
	if (get(from.x, from.y, from.z) != MapChipType::Clay) {
		return false;
	}

	// 伸ばせるのは元セルの前後左右 4 方向に各 1 セル
	const i32 source = flat_index(from.x, from.y, from.z);
	const i32 root = clayOrigin[source];
	const MapChipIndex origin = unflatten(root);
	if (to.y != origin.y || std::abs(to.x - origin.x) + std::abs(to.z - origin.z) != 1) {
		return false;
	}

	const i32 target = flat_index(to.x, to.y, to.z);
	if (chips[target] == MapChipType::GoalPiece) {
		// 伸ばす先がゴール条件オブジェクトなら伸びずにブロック全体がつながる(1 ブロックにつき 1 つ)
		if (clayPiece[source] != -1) {
			return false;
		}
		for (i32 i = 0; i < static_cast<i32>(chips.size()); ++i) {
			if (chips[i] == MapChipType::Clay && clayOrigin[i] == root) {
				clayPiece[i] = target;
				refresh_visual(i);
			}
		}
		++revision;
		return true;
	}
	if (chips[target] != MapChipType::Empty) {
		return false;
	}

	chips[target] = MapChipType::Clay;
	clayOrigin[target] = root;
	clayPiece[target] = clayPiece[source];
	++revision;
	refresh_visual(target);
	return true;
}

bool MapChipField::can_move_goal_piece(const MapChipIndex& from, const MapChipIndex& to) const {
	return !moving_cells(from, to).empty();
}

bool MapChipField::move_goal_piece(const MapChipIndex& from, const MapChipIndex& to) {
	const std::vector<i32> cells = moving_cells(from, to);
	if (cells.empty()) {
		return false;
	}
	const MapChipIndex delta{ to.x - from.x, 0, to.z - from.z };

	struct Moved {
		i32 target;
		MapChipType type;
		i32 origin;
		i32 piece;
	};
	std::vector<Moved> moved;
	for (const i32 cell : cells) {
		moved.push_back(Moved{ *shifted(cell, delta), chips[cell], clayOrigin[cell], clayPiece[cell] });
	}

	// 全部空けてからずらして置き直す(元セルと移動先の重なりを気にしなくてよい)
	for (const i32 cell : cells) {
		chips[cell] = MapChipType::Empty;
		clayOrigin[cell] = -1;
		clayPiece[cell] = -1;
		refresh_visual(cell);
	}
	for (const Moved& m : moved) {
		chips[m.target] = m.type;
		clayOrigin[m.target] = m.origin == -1 ? -1 : *shifted(m.origin, delta);
		clayPiece[m.target] = m.piece == -1 ? -1 : *shifted(m.piece, delta);
		refresh_visual(m.target);
	}
	++revision;
	return true;
}

std::vector<MapChipIndex> MapChipField::find_all(MapChipType type) const {
	std::vector<MapChipIndex> result;
	for (i32 y = 0; y < sizeY; ++y) {
		for (i32 z = 0; z < sizeZ; ++z) {
			for (i32 x = 0; x < sizeX; ++x) {
				if (chips[flat_index(x, y, z)] == type) {
					result.emplace_back(x, y, z);
				}
			}
		}
	}
	return result;
}

Reference<szg::StaticMeshInstance> MapChipField::visual_mut(const MapChipIndex& index) {
	if (visuals.empty() || !is_inside(index.x, index.y, index.z)) {
		return nullptr;
	}
	return visuals[flat_index(index.x, index.y, index.z)];
}

Vector3 MapChipField::to_world(i32 x, i32 y, i32 z) {
	return Vector3{ static_cast<r32>(x), static_cast<r32>(y), static_cast<r32>(z) };
}

std::optional<MapChipIndex> MapChipField::to_index(const Vector3& position) const {
	const MapChipIndex index{
		static_cast<i32>(std::floor(position.x + 0.5f)),
		static_cast<i32>(std::floor(position.y + 0.5f)),
		static_cast<i32>(std::floor(position.z + 0.5f)),
	};
	if (!is_inside(index.x, index.y, index.z)) {
		return std::nullopt;
	}
	return index;
}

bool MapChipField::contains(const MapChipIndex& index) const {
	return is_inside(index.x, index.y, index.z);
}

bool MapChipField::is_inside(i32 x, i32 y, i32 z) const {
	return 0 <= x && x < sizeX && 0 <= y && y < sizeY && 0 <= z && z < sizeZ;
}

i32 MapChipField::flat_index(i32 x, i32 y, i32 z) const {
	return x + sizeX * (z + sizeZ * y);
}

MapChipIndex MapChipField::unflatten(i32 flat) const {
	return MapChipIndex{ flat % sizeX, flat / (sizeX * sizeZ), (flat / sizeX) % sizeZ };
}

std::optional<i32> MapChipField::shifted(i32 flat, const MapChipIndex& delta) const {
	const MapChipIndex index = unflatten(flat);
	const i32 x = index.x + delta.x;
	const i32 y = index.y + delta.y;
	const i32 z = index.z + delta.z;
	if (!is_inside(x, y, z)) {
		return std::nullopt;
	}
	return flat_index(x, y, z);
}

std::vector<i32> MapChipField::moving_cells(const MapChipIndex& from, const MapChipIndex& to) const {
	if (from.y != to.y || std::abs(to.x - from.x) + std::abs(to.z - from.z) != 1) {
		return {};
	}
	if (get(from.x, from.y, from.z) != MapChipType::GoalPiece || !is_inside(to.x, to.y, to.z)) {
		return {};
	}

	// ピースと、それにつながった粘土の全セル
	const i32 piece = flat_index(from.x, from.y, from.z);
	std::vector<i32> cells{ piece };
	for (i32 i = 0; i < static_cast<i32>(chips.size()); ++i) {
		if (chips[i] == MapChipType::Clay && clayPiece[i] == piece) {
			cells.emplace_back(i);
		}
	}

	// 移動先は空か、一緒に動くセルが空ける場所
	const MapChipIndex delta{ to.x - from.x, 0, to.z - from.z };
	for (const i32 cell : cells) {
		const std::optional<i32> target = shifted(cell, delta);
		if (!target) {
			return {};
		}
		if (chips[*target] != MapChipType::Empty && std::find(cells.begin(), cells.end(), *target) == cells.end()) {
			return {};
		}
	}
	return cells;
}

void MapChipField::refresh_visual(i32 flat) {
	if (visuals.empty()) {
		return;
	}
	if (visuals[flat]) {
		visuals[flat]->destroy_self();
		visuals[flat].reset();
	}

	if (chips[flat] == MapChipType::Empty || !worldRoot) {
		return;
	}

	const MapChipIndex index = unflatten(flat);
	Reference<szg::StaticMeshInstance> cube = worldRoot->instantiate<szg::StaticMeshInstance>(nullptr, "Cube.obj");
	cube->transform_mut().set_translate(to_world(index.x, index.y, index.z));
	if (!cube->get_materials().empty()) {
		cube->get_materials()[0].color = ChipColor(chips[flat], clayPiece[flat] != -1);
	}

	visuals[flat] = cube;
}
