#include "MapChipField.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>

#include <json.hpp>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/CSV/CSVAssetBuilder.h>
#include <Engine/Assets/IAssetBuilder.h>
#include <Engine/Assets/PolygonMesh/PolygonMeshLibrary.h>
#include <Engine/Assets/Texture/TextureLibrary.h>
#include <Library/Math/ColorRGB.h>
#include <Library/Math/Quaternion.h>

namespace {

/// <summary>
/// チップ種類ごとの表示モデル設定
/// </summary>
struct ChipVisualSetting {
	MapChipType type;
	const char* assetPath;
	const char* meshName;
	r32 scale;
	r32 yOffset;
};

/// <summary>
/// チップ種類ごとの表示モデル設定の配列
/// </summary>
const std::array<ChipVisualSetting, 3> CHIP_VISUAL_SETTINGS{ {
	{ MapChipType::Clay, "[[game]]/clay/clay.obj", "clay.obj", 0.5f, -0.5f },
	{ MapChipType::GoalPiece, "[[game]]/goalPiece/goalPiece.obj", "goalPiece.obj", 0.5f, -0.5f },
	{ MapChipType::Goal, "[[game]]/goalOff/goalOff.obj", "goalOff.obj", 0.5f, -0.5f },
} };

constexpr const char* GOAL_ACTIVE_ASSET_PATH = "[[game]]/goal/goal.obj";
constexpr const char* GOAL_ACTIVE_MESH_NAME = "goal.obj";

// 動かせなかった通知の演出
constexpr r32 WARN_DURATION = 0.5f;
constexpr r32 WARN_BLINK_PERIOD = 0.1f;
constexpr i32 WARN_SHAKE_HOLD_FRAMES = 2; // 同じ側に留まるフレーム数(60 fps で 15 Hz)
constexpr r32 CROSS_SHAKE_AMPLITUDE = 0.035f; // 親 clay.obj のローカル単位(親 scale 0.5 なのでワールドでは半分)
constexpr r32 BLOCK_SHAKE_AMPLITUDE = 0.02f; // root ローカル単位(通常はワールドと同じ)

/// <summary>
/// チップ種類に対応する表示モデル設定を返す。
/// </summary>
const ChipVisualSetting* FindVisualSetting(MapChipType type) {
	const auto setting = std::find_if(
		CHIP_VISUAL_SETTINGS.begin(),
		CHIP_VISUAL_SETTINGS.end(),
		[type](const ChipVisualSetting& entry) { return entry.type == type; });
	return setting != CHIP_VISUAL_SETTINGS.end() ? &*setting : nullptr;
}

/// <summary>
/// "[[game]]/..." 形式の CSV パスが実在するか
/// </summary>
bool ExistsCsv(const std::string& path) {
	return std::filesystem::exists(szg::IAssetBuilder::ResolveFilePath(path, "csv"));
}

/// <summary>
/// "[[game]]/Map/StageNN" 形式のディレクトリ → stage.json の実パス(CSV と同じフォルダ)
/// </summary>
std::filesystem::path StageJsonPath(const std::string& directory) {
	return szg::IAssetBuilder::ResolveFilePath(std::format("{}/stage.json", directory), "csv");
}

/// <summary>
/// stage.json のルートオブジェクト(無い / 壊れている場合は空オブジェクト)
/// </summary>
nlohmann::json ReadStageJsonRoot(const std::filesystem::path& file) {
	if (std::ifstream ifs{ file }; ifs) {
		nlohmann::json existing = nlohmann::json::parse(ifs, nullptr, false);
		if (existing.is_object()) {
			return existing;
		}
	}
	return nlohmann::json::object();
}

bool WriteStageJsonRoot(const std::filesystem::path& file, const nlohmann::json& root) {
	std::filesystem::create_directories(file.parent_path());
	std::ofstream ofs{ file };
	if (!ofs) {
		szgWarning("MapChipField: failed to write \'{}\'", file.string());
		return false;
	}
	ofs << std::setw(1) << std::setfill('\t') << root;
	return true;
}

/// <summary>
/// "Position": [x, y, z] の検証と取り出し
/// </summary>
std::optional<MapChipIndex> ReadPosition(const nlohmann::json& entry) {
	const auto position = entry.is_object() ? entry.find("Position") : entry.end();
	if (position == entry.end() || !position->is_array() || position->size() != 3 ||
		!std::all_of(position->begin(), position->end(), [](const nlohmann::json& v) { return v.is_number(); })) {
		return std::nullopt;
	}
	return MapChipIndex{ position->at(0).get<i32>(), position->at(1).get<i32>(), position->at(2).get<i32>() };
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

void MapChipField::RegisterVisualAssets() {
	// Cube は不明なチップの代替表示、GoalManager の接続線でも使用する。
	szg::PolygonMeshLibrary::RegisterLoadQue("[[game]]/Cube.obj");
	szg::PolygonMeshLibrary::RegisterLoadQue("[[game]]/cross/cross.obj");
	for (const ChipVisualSetting& setting : CHIP_VISUAL_SETTINGS) {
		szg::PolygonMeshLibrary::RegisterLoadQue(setting.assetPath);
	}
	szg::PolygonMeshLibrary::RegisterLoadQue(GOAL_ACTIVE_ASSET_PATH);
	// 色 0 の clay.png は clay.obj の map_Kd で登録される。[[game]] タグだと Texture/ が挟まるので実パスで登録する
	for (i32 i = 1; i < ClayColor::Count; ++i) {
		szg::TextureLibrary::RegisterLoadQue(std::format("./Game/Assets/Models/clay/{}", ClayColor::Textures[i]));
	}
	// 伸ばして出た粘土の矢印付きテクスチャ(色 × 方向。make_arrow_textures.py で生成)
	for (i32 i = 0; i < ClayColor::Count; ++i) {
		for (const ClayFace::Entry& face : ClayFace::Table) {
			szg::TextureLibrary::RegisterLoadQue(std::format("./Game/Assets/Models/clay/{}", ClayColor::ArrowTexture(i, face.bit)));
		}
	}
}

bool MapChipField::load(const std::string& directory) {
	destroy_root();
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
		clayBlockedFaces.clear();
		clayColor.clear();
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

	// ゴール条件オブジェクトは 2 セル高。最上層にあれば 1 層足してから、真上のセルを上段として塞ぐ
	const i32 layerSize = sizeX * sizeZ;
	for (i32 i = layerSize * (sizeY - 1); i < static_cast<i32>(chips.size()); ++i) {
		if (chips[i] == MapChipType::GoalPiece) {
			chips.insert(chips.end(), static_cast<size_t>(layerSize), MapChipType::Empty);
			++sizeY;
			break;
		}
	}
	for (i32 i = 0; i < static_cast<i32>(chips.size()); ++i) {
		if (chips[i] != MapChipType::GoalPiece) {
			continue;
		}
		const i32 upper = i + layerSize;
		if (chips[upper] == MapChipType::Empty) {
			chips[upper] = MapChipType::GoalPieceUpper;
		}
		else {
			const MapChipIndex p = unflatten(i);
			szgWarning("MapChipField: cell above GoalPiece ({}, {}, {}) in \'{}\' is not empty (piece occupies 1 cell)", p.x, p.y, p.z, directory);
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

	// stage.json の塞がれた面と色。粘土でないセルの項目は無視
	clayBlockedFaces.assign(chips.size(), ClayFace::None);
	clayColor.assign(chips.size(), 0);
	i32 ignored = 0;
	for (const ClayRecord& record : LoadStageJsonClay(directory)) {
		const MapChipIndex& p = record.position;
		if (!is_inside(p.x, p.y, p.z) || chips[flat_index(p.x, p.y, p.z)] != MapChipType::Clay) {
			++ignored;
			continue;
		}
		clayBlockedFaces[flat_index(p.x, p.y, p.z)] |= record.blockedFaces;
		clayColor[flat_index(p.x, p.y, p.z)] = record.color;
	}
	szgWarningIf(ignored > 0, "MapChipField: {} Clay entries in \'{}/stage.json\' are not on a clay cell (ignored)", ignored, directory);

	// stage.json のプレイヤー初期位置。Y はその列の床に合わせる
	playerSpawn = LoadStageJsonPlayerSpawn(directory);
	if (playerSpawn) {
		MapChipIndex& p = playerSpawn->position;
		const std::optional<i32> floorY = is_inside(p.x, 0, p.z)
			? SnapToFloorY(p.x, p.y, p.z, sizeY, [this](i32 x, i32 y, i32 z) { return get(x, y, z); })
			: std::nullopt;
		if (!floorY) {
			szgWarning("MapChipField: PlayerSpawn ({}, {}, {}) in \'{}/stage.json\' has no empty cell in its column (ignored)", p.x, p.y, p.z, directory);
			playerSpawn.reset();
		}
		else {
			p.y = *floorY;
		}
	}
	return true;
}

void MapChipField::build(szg::WorldRoot& worldRoot_) {
	destroy_root();
	worldRoot = worldRoot_;
	if (chips.empty()) {
		return;
	}

	root = worldRoot->instantiate<szg::WorldInstance>(nullptr);
	root->transform_mut().set_translate(center());
	visuals.assign(chips.size(), Reference<szg::StaticMeshInstance>{});
	faceCrosses.assign(chips.size(), {});

	for (i32 i = 0; i < static_cast<i32>(chips.size()); ++i) {
		refresh_visual(i);
	}
}

void MapChipField::destroy_root() {
	cancel_visual_interpolation();
	if (root) {
		root->destroy_self();
		root.reset();
	}
	visuals.clear();
	faceCrosses.clear();
	warnings.clear();
}

void MapChipField::warn_blocked_face(const MapChipIndex& index, const MapChipIndex& direction) {
	if (!is_inside(index.x, index.y, index.z)) {
		return;
	}
	const i32 root = clayOrigin[flat_index(index.x, index.y, index.z)];
	const u8 bit = ClayFace::FromDirection(direction);
	if (root >= 0 && root < static_cast<i32>(chips.size()) && (clayBlockedFaces[root] & bit)) {
		begin_blocked_face_warning(root, bit);
	}
}

void MapChipField::warn_block_stuck(const MapChipIndex& index, const MapChipIndex& direction) {
	if (is_visual_interpolating() || visuals.empty() || !is_inside(index.x, index.y, index.z)) {
		return;
	}
	i32 flat = flat_index(index.x, index.y, index.z);
	if (chips[flat] == MapChipType::GoalPieceUpper) {
		flat = shifted(flat, MapChipIndex{ 0, -1, 0 }).value_or(flat);
	}

	// 粘土は同じ元セルの全セル、ピースは下段・上段とつながった粘土の全セル
	std::vector<i32> cells;
	if (chips[flat] == MapChipType::Clay) {
		for (i32 i = 0; i < static_cast<i32>(chips.size()); ++i) {
			if (chips[i] == MapChipType::Clay && clayOrigin[i] == clayOrigin[flat]) {
				cells.push_back(i);
			}
		}
	}
	else if (chips[flat] == MapChipType::GoalPiece) {
		cells.push_back(flat);
		for (i32 i = 0; i < static_cast<i32>(chips.size()); ++i) {
			if (chips[i] == MapChipType::Clay && clayPiece[i] == flat) {
				cells.push_back(i);
			}
		}
	}

	const Vector3 shakeAxis{ static_cast<r32>(direction.x), 0.0f, static_cast<r32>(direction.z) };
	for (const i32 cell : cells) {
		if (visuals[cell]) {
			begin_warning(cell, visuals[cell], shakeAxis, BLOCK_SHAKE_AMPLITUDE, false);
		}
	}
}

void MapChipField::update_warnings(r32 deltaSeconds) {
	for (auto it = warnings.begin(); it != warnings.end();) {
		WarningEffect& warning = *it;
		warning.elapsed += std::max(deltaSeconds, 0.0f);
		if (warning.elapsed >= WARN_DURATION) {
			end_warning(warning);
			it = warnings.erase(it);
			continue;
		}

		if (!warning.baseColors.empty()) {
			const bool red = std::fmod(warning.elapsed, WARN_BLINK_PERIOD) < WARN_BLINK_PERIOD * 0.5f;
			std::vector<szg::IMultiMeshInstance::Material>& materials = warning.visual->get_materials();
			for (size_t i = 0; i < materials.size() && i < warning.baseColors.size(); ++i) {
				materials[i].color = red ? CColorRGB::RED : warning.baseColors[i];
			}
		}
		++warning.frames;
		const r32 sign = (warning.frames / WARN_SHAKE_HOLD_FRAMES) % 2 == 0 ? 1.0f : -1.0f;
		warning.visual->transform_mut().set_translate(warning.basePosition + warning.shakeAxis * (sign * warning.shakeAmplitude));
		++it;
	}
}

void MapChipField::begin_blocked_face_warning(i32 root, u8 bit) {
	if (root < 0 || root >= static_cast<i32>(faceCrosses.size())) {
		return;
	}
	for (size_t i = 0; i < ClayFace::Table.size(); ++i) {
		Reference<szg::StaticMeshInstance> cross = faceCrosses[root][i];
		if (ClayFace::Table[i].bit != bit || !cross) {
			continue;
		}
		// 面に沿った水平方向に揺らす
		const MapChipIndex direction = ClayFace::Table[i].direction;
		const Vector3 shakeAxis{ static_cast<r32>(std::abs(direction.z)), 0.0f, static_cast<r32>(std::abs(direction.x)) };
		begin_warning(root, cross, shakeAxis, CROSS_SHAKE_AMPLITUDE, true);
		return;
	}
}

void MapChipField::begin_warning(i32 flat, Reference<szg::StaticMeshInstance> visual, const Vector3& shakeAxis, r32 shakeAmplitude, bool blink) {
	for (auto it = warnings.begin(); it != warnings.end(); ++it) {
		if (it->visual == visual) {
			end_warning(*it);
			warnings.erase(it);
			break;
		}
	}
	WarningEffect warning{
		.flat = flat,
		.visual = visual,
		.basePosition = visual->transform_imm().get_translate(),
		.shakeAxis = shakeAxis,
		.shakeAmplitude = shakeAmplitude,
	};
	if (blink) {
		for (const szg::IMultiMeshInstance::Material& material : visual->get_materials()) {
			warning.baseColors.push_back(material.color);
		}
	}
	warnings.push_back(std::move(warning));
}

void MapChipField::end_warning(WarningEffect& warning) {
	std::vector<szg::IMultiMeshInstance::Material>& materials = warning.visual->get_materials();
	for (size_t i = 0; i < materials.size() && i < warning.baseColors.size(); ++i) {
		materials[i].color = warning.baseColors[i];
	}
	warning.visual->transform_mut().set_translate(warning.basePosition);
}

void MapChipField::end_warnings() {
	for (WarningEffect& warning : warnings) {
		end_warning(warning);
	}
	warnings.clear();
}

void MapChipField::update_visual_interpolation(r32 deltaSeconds) {
	if (visualInterpolationSteps.empty()) {
		return;
	}

	VisualInterpolationStep& step = visualInterpolationSteps.front();
	visualInterpolationElapsed += std::max(deltaSeconds, 0.0f);
	const r32 duration = std::max(step.duration, 0.001f);
	const r32 t = std::clamp(visualInterpolationElapsed / duration, 0.0f, 1.0f);
	const r32 eased = t * t * (3.0f - 2.0f * t);
	for (VisualInterpolation& interpolation : step.entries) {
		if (interpolation.visual) {
			interpolation.visual->transform_mut().set_translate(Vector3::Lerp(
				interpolation.startPosition,
				interpolation.targetPosition,
				eased));
		}
	}
	if (t >= 1.0f) {
		visualInterpolationSteps.pop_front();
		visualInterpolationElapsed = 0.0f;
	}
}

void MapChipField::begin_visual_interpolation(
	const std::vector<i32>& targetCells,
	const std::vector<VisualMove>& moves) {
	cancel_visual_interpolation();
	// 振動のオフセットを戻してから現在位置を控える
	end_warnings();
	Vector3 totalOffset = CVector3::ZERO;
	for (const VisualMove& move : moves) {
		if (move.duration > 0.0f) {
			totalOffset += move.offset;
			visualInterpolationSteps.push_back(VisualInterpolationStep{ .duration = move.duration });
		}
	}
	if (visualInterpolationSteps.empty()) {
		return;
	}

	for (const i32 target : targetCells) {
		if (target < 0 || target >= static_cast<i32>(visuals.size()) || !visuals[target]) {
			continue;
		}
		Vector3 position = visuals[target]->transform_imm().get_translate() - totalOffset;
		visuals[target]->transform_mut().set_translate(position);
		size_t stepIndex = 0;
		for (const VisualMove& move : moves) {
			if (move.duration <= 0.0f) {
				continue;
			}
			visualInterpolationSteps[stepIndex++].entries.push_back(VisualInterpolation{
				.visual = visuals[target],
				.startPosition = position,
				.targetPosition = position + move.offset,
			});
			position += move.offset;
		}
	}
}

void MapChipField::cancel_visual_interpolation() {
	// 後の段階ほど最終位置に近いので、順に置けば最後の段階の target で終わる
	for (VisualInterpolationStep& step : visualInterpolationSteps) {
		for (VisualInterpolation& interpolation : step.entries) {
			if (interpolation.visual) {
				interpolation.visual->transform_mut().set_translate(interpolation.targetPosition);
			}
		}
	}
	visualInterpolationSteps.clear();
	visualInterpolationElapsed = 0.0f;
}

MapChipType MapChipField::get(i32 x, i32 y, i32 z) const {
	return is_inside(x, y, z) ? chips[flat_index(x, y, z)] : MapChipType::Empty;
}

void MapChipField::set(i32 x, i32 y, i32 z, MapChipType type, u8 color) {
	if (!is_inside(x, y, z)) {
		return;
	}
	cancel_visual_interpolation();
	const i32 i = flat_index(x, y, z);
	chips[i] = type;
	clayOrigin[i] = type == MapChipType::Clay ? i : -1;
	clayPiece[i] = -1;
	clayBlockedFaces[i] = ClayFace::None;
	clayColor[i] = static_cast<u8>(type == MapChipType::Clay ? std::min<i32>(color, ClayColor::Count - 1) : 0);
	++revision;
	refresh_visual(i);
}

void MapChipField::restore(const Cells& source) {
	if (source.chips.size() != chips.size()) {
		szgWarning("MapChipField: restore size mismatch ({} != {})", source.chips.size(), chips.size());
		return;
	}
	cancel_visual_interpolation();
	for (i32 i = 0; i < static_cast<i32>(chips.size()); ++i) {
		if (chips[i] == source.chips[i] && clayOrigin[i] == source.clayOrigin[i] &&
			clayPiece[i] == source.clayPiece[i] && clayBlockedFaces[i] == source.clayBlockedFaces[i] &&
			clayColor[i] == source.clayColor[i]) {
			continue;
		}
		chips[i] = source.chips[i];
		clayOrigin[i] = source.clayOrigin[i];
		clayPiece[i] = source.clayPiece[i];
		clayBlockedFaces[i] = source.clayBlockedFaces[i];
		clayColor[i] = source.clayColor[i];
		refresh_visual(i);
	}
	++revision;
}

bool MapChipField::stretch_clay(
	const MapChipIndex& from,
	const MapChipIndex& to,
	r32 visualMoveDuration) {
	if (!is_inside(from.x, from.y, from.z) || !is_inside(to.x, to.y, to.z)) {
		return false;
	}
	if (std::abs(to.x - from.x) + std::abs(to.y - from.y) + std::abs(to.z - from.z) != 1) {
		return false;
	}
	if (get(from.x, from.y, from.z) != MapChipType::Clay) {
		return false;
	}

	// コアは前後左右へ分岐できる。子はコアから伸びた直線の外向きにだけ伸ばせる。
	const i32 source = flat_index(from.x, from.y, from.z);
	const i32 root = clayOrigin[source];
	if (root < 0 || root >= static_cast<i32>(chips.size())) {
		return false;
	}
	const MapChipIndex origin = unflatten(root);
	if (to.y != origin.y) {
		return false;
	}
	MapChipIndex stretchDirection{
		to.x - from.x,
		0,
		to.z - from.z,
	};
	if (source != root) {
		const i32 branchX = from.x - origin.x;
		const i32 branchZ = from.z - origin.z;
		// 子は必ずコアと同じX軸またはZ軸上にあり、その外向きだけを許可する。
		if ((branchX != 0 && branchZ != 0) || (branchX == 0 && branchZ == 0)) {
			return false;
		}
		const i32 stretchX = branchX == 0 ? 0 : (branchX < 0 ? -1 : 1);
		const i32 stretchZ = branchZ == 0 ? 0 : (branchZ < 0 ? -1 : 1);
		if (to.x - from.x != stretchX || to.z - from.z != stretchZ) {
			return false;
		}
		stretchDirection = { stretchX, 0, stretchZ };
	}
	// 塞がれた面からは伸びず、ゴール条件オブジェクトにもつながらない
	const u8 stretchFace = ClayFace::FromDirection(stretchDirection);
	if (clayBlockedFaces[root] & stretchFace) {
		begin_blocked_face_warning(root, stretchFace);
		return false;
	}

	const i32 target = flat_index(to.x, to.y, to.z);
	if (chips[target] == MapChipType::GoalPiece || chips[target] == MapChipType::GoalPieceUpper) {
		// 伸ばす先がゴール条件オブジェクト(上段でも可)なら伸びずにブロック全体がつながる(1 ブロックにつき 1 つ)。つながり先は下段のセル
		if (clayPiece[source] != -1) {
			return false;
		}
		const i32 piece = chips[target] == MapChipType::GoalPiece ? target : *shifted(target, MapChipIndex{ 0, -1, 0 });
		cancel_visual_interpolation();
		// 伸ばしたブロックと、粘土づたいに面で接している未接続の粘土を全部このピースにつなぐ(上下も含む 6 方向)
		constexpr std::array<MapChipIndex, 6> kNeighbors{ {
			{ 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
		} };
		std::vector<i32> open{ source };
		clayPiece[source] = piece;
		while (!open.empty()) {
			const i32 cell = open.back();
			open.pop_back();
			for (const MapChipIndex& direction : kNeighbors) {
				const std::optional<i32> next = shifted(cell, direction);
				if (next && chips[*next] == MapChipType::Clay && clayPiece[*next] == -1) {
					clayPiece[*next] = piece;
					open.push_back(*next);
				}
			}
		}
		++revision;
		return true;
	}
	if (chips[target] != MapChipType::Empty) {
		return false;
	}

	cancel_visual_interpolation();
	chips[target] = MapChipType::Clay;
	clayOrigin[target] = root;
	clayPiece[target] = clayPiece[source];
	clayColor[target] = clayColor[source];
	++revision;
	refresh_visual(target);
	begin_visual_interpolation(
		std::vector<i32>{ target },
		{ VisualMove{ to_world(to.x - from.x, to.y - from.y, to.z - from.z), visualMoveDuration } });
	return true;
}

bool MapChipField::can_move_goal_piece(const MapChipIndex& from, const MapChipIndex& to) const {
	return !moving_cells(from, to).empty();
}

std::optional<MapChipIndex> MapChipField::move_goal_piece(
	const MapChipIndex& from,
	const MapChipIndex& to,
	r32 visualMoveDuration) {
	const std::vector<i32> cells = moving_cells(from, to);
	if (cells.empty()) {
		return std::nullopt;
	}
	const MapChipIndex delta{ to.x - from.x, 0, to.z - from.z };
	std::vector<i32> targetCells = relocate_cells(cells, delta);

	// グループ全セルの下が空(または自グループ)の間、落下マス数を増やす。y=0 は shifted が nullopt になるので床扱い
	i32 fallCount = 0;
	const auto isSupported = [&](const MapChipIndex& fallDelta) {
		for (const i32 cell : targetCells) {
			const std::optional<i32> below = shifted(cell, fallDelta);
			if (!below) {
				return true;
			}
			if (chips[*below] != MapChipType::Empty &&
				std::find(targetCells.begin(), targetCells.end(), *below) == targetCells.end()) {
				return true;
			}
		}
		return false;
	};
	while (!isSupported(MapChipIndex{ 0, -(fallCount + 1), 0 })) {
		++fallCount;
	}
	if (fallCount > 0) {
		targetCells = relocate_cells(targetCells, MapChipIndex{ 0, -fallCount, 0 });
	}
	++revision;

	std::vector<VisualMove> moves{ VisualMove{ to_world(delta.x, delta.y, delta.z), visualMoveDuration } };
	if (fallCount > 0) {
		moves.push_back(VisualMove{ to_world(0, -fallCount, 0), visualMoveDuration * static_cast<r32>(fallCount) });
	}
	begin_visual_interpolation(targetCells, moves);
	return MapChipIndex{ to.x, to.y - fallCount, to.z };
}

std::vector<i32> MapChipField::relocate_cells(const std::vector<i32>& cells, const MapChipIndex& delta) {
	struct Moved {
		i32 target;
		MapChipType type;
		i32 origin;
		i32 piece;
		u8 faces;
		u8 color;
	};
	std::vector<Moved> moved;
	for (const i32 cell : cells) {
		moved.push_back(Moved{ *shifted(cell, delta), chips[cell], clayOrigin[cell], clayPiece[cell], clayBlockedFaces[cell], clayColor[cell] });
	}

	// 全部空けてからずらして置き直す(元セルと移動先の重なりを気にしなくてよい)
	for (const i32 cell : cells) {
		chips[cell] = MapChipType::Empty;
		clayOrigin[cell] = -1;
		clayPiece[cell] = -1;
		clayBlockedFaces[cell] = ClayFace::None;
		clayColor[cell] = 0;
		refresh_visual(cell);
	}
	for (const Moved& m : moved) {
		chips[m.target] = m.type;
		clayOrigin[m.target] = m.origin == -1 ? -1 : *shifted(m.origin, delta);
		clayPiece[m.target] = m.piece == -1 ? -1 : *shifted(m.piece, delta);
		clayBlockedFaces[m.target] = m.faces;
		clayColor[m.target] = m.color;
		refresh_visual(m.target);
	}
	std::vector<i32> targetCells;
	targetCells.reserve(moved.size());
	for (const Moved& m : moved) {
		targetCells.push_back(m.target);
	}
	return targetCells;
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

bool MapChipField::is_clay_core(const MapChipIndex& index) const {
	if (!is_inside(index.x, index.y, index.z)) {
		return false;
	}
	const i32 flat = flat_index(index.x, index.y, index.z);
	return chips[flat] == MapChipType::Clay && clayOrigin[flat] == flat;
}

u8 MapChipField::blocked_faces(const MapChipIndex& index) const {
	if (!is_inside(index.x, index.y, index.z)) {
		return ClayFace::None;
	}
	const i32 root = clayOrigin[flat_index(index.x, index.y, index.z)];
	return root == -1 ? ClayFace::None : clayBlockedFaces[root];
}

u8 MapChipField::clay_color(const MapChipIndex& index) const {
	return is_inside(index.x, index.y, index.z) ? clayColor[flat_index(index.x, index.y, index.z)] : 0;
}

std::vector<ClayRecord> MapChipField::LoadStageJsonClay(const std::string& directory) {
	std::vector<ClayRecord> result;
	const std::filesystem::path file = StageJsonPath(directory);
	std::ifstream ifs{ file };
	if (!ifs) {
		return result;
	}

	const nlohmann::json root = nlohmann::json::parse(ifs, nullptr, false);
	if (!root.is_object()) {
		szgWarning("MapChipField: \'{}\' is not a json object", file.string());
		return result;
	}
	const auto clay = root.find("Clay");
	if (clay == root.end()) {
		return result;
	}
	if (!clay->is_array()) {
		szgWarning("MapChipField: \'{}\' \"Clay\" must be an array", file.string());
		return result;
	}

	for (const nlohmann::json& entry : *clay) {
		const std::optional<MapChipIndex> position = ReadPosition(entry);
		if (!position) {
			szgWarning("MapChipField: \'{}\' Clay entry has invalid \"Position\": {}", file.string(), entry.dump());
			continue;
		}
		ClayRecord record{ *position, ClayFace::None };
		if (const auto faces = entry.find("BlockedFaces"); faces != entry.end() && faces->is_array()) {
			for (const nlohmann::json& face : *faces) {
				const u8 bit = face.is_string() ? ClayFace::FromName(face.get<std::string>()) : ClayFace::None;
				szgWarningIf(bit == ClayFace::None, "MapChipField: \'{}\' unknown face {}", file.string(), face.dump());
				record.blockedFaces |= bit;
			}
		}
		if (const auto color = entry.find("Color"); color != entry.end()) {
			const i32 value = color->is_number_integer() ? color->get<i32>() : -1;
			if (0 <= value && value < ClayColor::Count) {
				record.color = static_cast<u8>(value);
			}
			else {
				szgWarning("MapChipField: \'{}\' invalid \"Color\" {} (0..{})", file.string(), color->dump(), ClayColor::Count - 1);
			}
		}
		result.push_back(record);
	}
	return result;
}

bool MapChipField::SaveStageJsonClay(const std::string& directory, const std::vector<ClayRecord>& records) {
	const std::filesystem::path file = StageJsonPath(directory);
	// 他のキーを残すため既存を読む
	nlohmann::json root = ReadStageJsonRoot(file);

	nlohmann::json clay = nlohmann::json::array();
	for (const ClayRecord& record : records) {
		nlohmann::json names = nlohmann::json::array();
		for (const ClayFace::Entry& entry : ClayFace::Table) {
			if (record.blockedFaces & entry.bit) {
				names.push_back(entry.name);
			}
		}
		nlohmann::json item = nlohmann::json::object();
		item["Position"] = { record.position.x, record.position.y, record.position.z };
		item["BlockedFaces"] = std::move(names);
		item["Color"] = static_cast<i32>(record.color);
		clay.push_back(std::move(item));
	}
	root["Clay"] = std::move(clay);
	return WriteStageJsonRoot(file, root);
}

std::optional<PlayerSpawnRecord> MapChipField::LoadStageJsonPlayerSpawn(const std::string& directory) {
	const std::filesystem::path file = StageJsonPath(directory);
	std::ifstream ifs{ file };
	if (!ifs) {
		return std::nullopt;
	}

	const nlohmann::json root = nlohmann::json::parse(ifs, nullptr, false);
	if (!root.is_object()) {
		szgWarning("MapChipField: \'{}\' is not a json object", file.string());
		return std::nullopt;
	}
	const auto spawn = root.find("PlayerSpawn");
	if (spawn == root.end()) {
		return std::nullopt;
	}

	const std::optional<MapChipIndex> position = ReadPosition(*spawn);
	if (!position) {
		szgWarning("MapChipField: \'{}\' \"PlayerSpawn\" has invalid \"Position\": {}", file.string(), spawn->dump());
		return std::nullopt;
	}
	PlayerSpawnRecord record{ *position };
	if (const auto direction = spawn->find("Direction"); direction != spawn->end()) {
		const u8 bit = direction->is_string() ? ClayFace::FromName(direction->get<std::string>()) : ClayFace::None;
		if (bit != ClayFace::None) {
			record.direction = bit;
		}
		else {
			szgWarning("MapChipField: \'{}\' \"PlayerSpawn\" has unknown \"Direction\" {} (+Z)", file.string(), direction->dump());
		}
	}
	return record;
}

bool MapChipField::SaveStageJsonPlayerSpawn(const std::string& directory, const std::optional<PlayerSpawnRecord>& spawn) {
	const std::filesystem::path file = StageJsonPath(directory);
	nlohmann::json root = ReadStageJsonRoot(file);

	if (spawn) {
		nlohmann::json item = nlohmann::json::object();
		item["Position"] = { spawn->position.x, spawn->position.y, spawn->position.z };
		item["Direction"] = ClayFace::ToName(spawn->direction);
		root["PlayerSpawn"] = std::move(item);
	}
	else {
		root.erase("PlayerSpawn");
	}
	return WriteStageJsonRoot(file, root);
}

std::array<Reference<szg::StaticMeshInstance>, 4> MapChipField::AttachFaceCrosses(szg::WorldRoot& worldRoot_, Reference<szg::WorldInstance> parent, u8 blockedFaces, r32 halfSize, r32 bottomY) {
	std::array<Reference<szg::StaticMeshInstance>, 4> crosses{};
	// cross.obj は底面原点・幅 2・高さ 2 で +Z を向く。親の面と同じ枠なので halfSize で等倍し、面の外向きへ回す
	for (size_t i = 0; i < ClayFace::Table.size(); ++i) {
		const ClayFace::Entry& face = ClayFace::Table[i];
		if (!(blockedFaces & face.bit)) {
			continue;
		}
		Reference<szg::StaticMeshInstance> cross = worldRoot_.instantiate<szg::StaticMeshInstance>(parent, "cross.obj");
		const Vector3 direction = to_world(face.direction.x, face.direction.y, face.direction.z);
		cross->transform_mut().set_scale(Vector3{ halfSize, halfSize, halfSize });
		cross->transform_mut().set_translate(direction * halfSize + Vector3{ 0.0f, bottomY, 0.0f });
		cross->transform_mut().set_quaternion(Quaternion::LookForward(direction));
		crosses[i] = cross;
	}
	return crosses;
}

Reference<szg::StaticMeshInstance> MapChipField::visual_mut(const MapChipIndex& index) {
	if (visuals.empty() || !is_inside(index.x, index.y, index.z)) {
		return nullptr;
	}
	return visuals[flat_index(index.x, index.y, index.z)];
}

bool MapChipField::set_goal_active(const MapChipIndex& index, bool active) {
	if (get(index.x, index.y, index.z) != MapChipType::Goal) {
		return false;
	}
	const i32 flat = flat_index(index.x, index.y, index.z);
	const ChipVisualSetting* offSetting = FindVisualSetting(MapChipType::Goal);
	const char* meshName = active ? GOAL_ACTIVE_MESH_NAME : (offSetting ? offSetting->meshName : "goalOff.obj");
	if (visuals.empty()) {
		return false;
	}
	if (visuals[flat] && visuals[flat]->key_id() == meshName) {
		return true;
	}

	// 描画Executorはインスタンス生成時のメッシュ単位で登録されるため、reset_meshではなく再生成する。
	refresh_visual(flat, active);
	return static_cast<bool>(visuals[flat]);
}

Vector3 MapChipField::to_world(i32 x, i32 y, i32 z) {
	return Vector3{ static_cast<r32>(x), static_cast<r32>(y), static_cast<r32>(z) };
}

Vector3 MapChipField::center() const {
	return to_world(sizeX - 1, sizeY - 1, sizeZ - 1) * 0.5f;
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

u8 MapChipField::clay_stretch_face(i32 flat) const {
	const i32 origin = clayOrigin[flat];
	if (chips[flat] != MapChipType::Clay || origin < 0 || origin == flat) {
		return ClayFace::None;
	}
	// 伸ばして出たセルはコアと同じ X 軸か Z 軸上にあるので、コアからの向きが伸ばした方向
	const MapChipIndex self = unflatten(flat);
	const MapChipIndex core = unflatten(origin);
	const i32 dx = self.x - core.x;
	const i32 dz = self.z - core.z;
	if ((dx != 0) == (dz != 0)) {
		return ClayFace::None;
	}
	return ClayFace::FromDirection({ dx == 0 ? 0 : (dx < 0 ? -1 : 1), 0, dz == 0 ? 0 : (dz < 0 ? -1 : 1) });
}

std::vector<i32> MapChipField::moving_cells(const MapChipIndex& from, const MapChipIndex& to) const {
	if (from.y != to.y || std::abs(to.x - from.x) + std::abs(to.z - from.z) != 1) {
		return {};
	}
	if (get(from.x, from.y, from.z) != MapChipType::GoalPiece || !is_inside(to.x, to.y, to.z)) {
		return {};
	}

	// ピース(下段と上段)と、それにつながった粘土の全セル
	const i32 piece = flat_index(from.x, from.y, from.z);
	std::vector<i32> cells{ piece };
	if (const std::optional<i32> upper = shifted(piece, MapChipIndex{ 0, 1, 0 });
		upper && chips[*upper] == MapChipType::GoalPieceUpper) {
		cells.emplace_back(*upper);
	}
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

void MapChipField::refresh_visual(i32 flat, bool goalActive) {
	if (visuals.empty()) {
		return;
	}
	if (visuals[flat]) {
		// destroy_self は親の children から自分を外さないので、root に無効な参照が残らないよう先に外す。
		visuals[flat]->reparent(nullptr, true);
		visuals[flat]->destroy_self();
		visuals[flat].reset();
	}
	// 表示(と子の cross)は消えるので、そのセルの演出は参照を捨てる(復元は不要)
	faceCrosses[flat] = {};
	std::erase_if(warnings, [flat](const WarningEffect& warning) { return warning.flat == flat; });

	// 上段はピースのモデル(下段)に含まれるので表示を持たない
	if (chips[flat] == MapChipType::Empty || chips[flat] == MapChipType::GoalPieceUpper || !root) {
		return;
	}

	//マップチップのグリッド座標を取得
	const MapChipIndex index = unflatten(flat);

	// チップ種類ごとの表示モデル設定を探す。見つからなければ Cube.obj で代替表示
	const ChipVisualSetting* setting = FindVisualSetting(chips[flat]);

	// 見つからなければ Cube.obj で代替表示
	const char* meshName = chips[flat] == MapChipType::Goal && goalActive
		? GOAL_ACTIVE_MESH_NAME
		: (setting ? setting->meshName : "Cube.obj");
	Reference<szg::StaticMeshInstance> visual =
		worldRoot->instantiate<szg::StaticMeshInstance>(root, meshName);

	// 親 root はステージ中央に置くので、子のローカル座標は中央基準
	Vector3 localPosition = to_world(index.x, index.y, index.z) - center();
	if (setting) {
		visual->transform_mut().set_scale(Vector3{ setting->scale, setting->scale, setting->scale });
		localPosition.y += setting->yOffset;
	}
	visual->transform_mut().set_translate(localPosition);
	// 粘土はインスタンス単位でテクスチャを差し替える。伸ばして出たセルは矢印付き、コアは色のみ(0 は clay.obj 既定の clay.png のまま)
	if (chips[flat] == MapChipType::Clay && !visual->get_materials().empty()) {
		if (const u8 face = clay_stretch_face(flat); face != ClayFace::None) {
			visual->get_materials()[0].texture = szg::TextureLibrary::GetTexture(ClayColor::ArrowTexture(clayColor[flat], face));
		}
		else if (clayColor[flat] != 0) {
			visual->get_materials()[0].texture = szg::TextureLibrary::GetTexture(ClayColor::Textures[clayColor[flat]]);
		}
	}
	// 塞がれた面のバツ印は元セルにだけ付ける(clay.obj ローカルは半幅 1・底面 0)
	if (chips[flat] == MapChipType::Clay && clayOrigin[flat] == flat) {
		faceCrosses[flat] = AttachFaceCrosses(*worldRoot, visual, clayBlockedFaces[flat], 1.0f, 0.0f);
	}

	visuals[flat] = visual;
}
