#include "MapChipField.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/CSV/CSVAssetBuilder.h>
#include <Engine/Assets/IAssetBuilder.h>
#include <Library/Math/ColorRGB.h>

namespace {

/// <summary>
/// チップ種類ごとの表示色
/// </summary>
ColorRGB ChipColor(MapChipType type) {
	switch (type) {
	case MapChipType::Clay:
		return ColorRGB{ 0.55f, 0.35f, 0.20f };
	case MapChipType::GoalPiece:
		return CColorRGB::YELLOW;
	default:
		return CColorRGB::WHITE;
	}
}

} // namespace

bool MapChipField::load(const std::string& directory) {
	for (auto& visual : visuals) {
		if (visual) {
			visual->destroy_self();
		}
	}
	visuals.clear();

	std::vector<szg::CSVAsset<i32>> layers;
	for (i32 i = 1; ; ++i) {
		std::filesystem::path file = szg::IAssetBuilder::ResolveFilePath(std::format("{}/layer{}.csv", directory, i), "csv");
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
	return true;
}

void MapChipField::build(szg::WorldRoot& worldRoot_) {
	worldRoot = worldRoot_;
	visuals.assign(chips.size(), Reference<szg::StaticMeshInstance>{});

	for (i32 y = 0; y < sizeY; ++y) {
		for (i32 z = 0; z < sizeZ; ++z) {
			for (i32 x = 0; x < sizeX; ++x) {
				refresh_visual(x, y, z);
			}
		}
	}
}

MapChipType MapChipField::get(i32 x, i32 y, i32 z) const {
	return is_inside(x, y, z) ? chips[flat_index(x, y, z)] : MapChipType::Empty;
}

void MapChipField::set(i32 x, i32 y, i32 z, MapChipType type) {
	if (!is_inside(x, y, z)) {
		return;
	}
	chips[flat_index(x, y, z)] = type;
	if (!visuals.empty()) {
		refresh_visual(x, y, z);
	}
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

bool MapChipField::is_inside(i32 x, i32 y, i32 z) const {
	return 0 <= x && x < sizeX && 0 <= y && y < sizeY && 0 <= z && z < sizeZ;
}

i32 MapChipField::flat_index(i32 x, i32 y, i32 z) const {
	return x + sizeX * (z + sizeZ * y);
}

void MapChipField::refresh_visual(i32 x, i32 y, i32 z) {
	const i32 i = flat_index(x, y, z);

	if (visuals[i]) {
		visuals[i]->destroy_self();
		visuals[i].reset();
	}

	if (chips[i] == MapChipType::Empty || !worldRoot) {
		return;
	}

	Reference<szg::StaticMeshInstance> cube = worldRoot->instantiate<szg::StaticMeshInstance>(nullptr, "Cube.obj");
	cube->transform_mut().set_translate(to_world(x, y, z));
	if (!cube->get_materials().empty()) {
		cube->get_materials()[0].color = ChipColor(chips[i]);
	}

	visuals[i] = cube;
}
