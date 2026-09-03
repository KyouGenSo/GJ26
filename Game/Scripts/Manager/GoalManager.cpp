#include "GoalManager.h"

#include <algorithm>
#include <cmath>
#include <queue>

#include <Engine/Application/Logger.h>
#include <Library/Math/ColorRGB.h>

#include "Scripts/Instance/Player/Player.h"

namespace {

constexpr r32 kLinkThickness = 0.2f;

} // namespace

void GoalManager::setup(Reference<MapChipField> field_, Reference<szg::WorldRoot> worldRoot_) {
	field = field_;
	worldRoot = worldRoot_;
}

void GoalManager::set_player(Reference<const Player> player_) {
	player = player_;
}

void GoalManager::post_update() {
	if (!field) {
		return;
	}
	if (field->version() != lastVersion) {
		rebuild();
	}

	// 操作対象は後から差し替えられるので毎フレーム取り直す
	const Reference<const szg::WorldInstance> instance = player ? player->get_world_instance_imm() : nullptr;
	const bool wasCleared = cleared;
	cleared = goalOpen && instance && goal && field->to_index(instance->world_position()) == *goal;
	if (cleared && !wasCleared) {
		szgInformation("GoalManager: stage clear");
	}
}

void GoalManager::rebuild() {
	lastVersion = field->version();

	for (auto& link : links) {
		if (link) {
			link->destroy_self();
		}
	}
	links.clear();

	const std::vector<MapChipIndex> pieces = field->find_all(MapChipType::GoalPiece);
	std::vector<std::vector<size_t>> adjacency(pieces.size());
	for (size_t i = 0; i < pieces.size(); ++i) {
		for (size_t j = i + 1; j < pieces.size(); ++j) {
			if (!is_connected(pieces[i], pieces[j])) {
				continue;
			}
			adjacency[i].emplace_back(j);
			adjacency[j].emplace_back(i);
			create_link(pieces[i], pieces[j]);
		}
	}

	// 全ピースが 1 つの連結成分か
	std::vector<bool> visited(pieces.size(), false);
	std::queue<size_t> queue;
	size_t visitedCount = 0;
	if (!pieces.empty()) {
		visited[0] = true;
		queue.push(0);
	}
	while (!queue.empty()) {
		const size_t current = queue.front();
		queue.pop();
		++visitedCount;
		for (size_t next : adjacency[current]) {
			if (visited[next]) {
				continue;
			}
			visited[next] = true;
			queue.push(next);
		}
	}
	goalOpen = visitedCount == pieces.size();

	const std::vector<MapChipIndex> goals = field->find_all(MapChipType::Goal);
	if (goals.size() > 1) {
		szgWarning("GoalManager: {} goals found. Only the first is used.", goals.size());
	}
	goal.reset();
	if (!goals.empty()) {
		goal = goals.front();
		if (Reference<szg::StaticMeshInstance> visual = field->visual_mut(*goal)) {
			visual->set_active(goalOpen);
		}
	}
}

bool GoalManager::is_connected(const MapChipIndex& a, const MapChipIndex& b) const {
	// 同じ高さで、X か Z のどちらか一方だけが揃っている
	if (a.y != b.y || (a.x == b.x) == (a.z == b.z)) {
		return false;
	}
	const bool alongX = a.z == b.z;
	const i32 begin = std::min(alongX ? a.x : a.z, alongX ? b.x : b.z);
	const i32 end = std::max(alongX ? a.x : a.z, alongX ? b.x : b.z);
	for (i32 i = begin + 1; i < end; ++i) {
		const MapChipType chip = alongX ? field->get(i, a.y, a.z) : field->get(a.x, a.y, i);
		if (chip != MapChipType::Empty && chip != MapChipType::Goal) {
			return false;
		}
	}
	return true;
}

void GoalManager::create_link(const MapChipIndex& a, const MapChipIndex& b) {
	if (!worldRoot) {
		return;
	}
	const Vector3 from = MapChipField::to_world(a.x, a.y, a.z);
	const Vector3 to = MapChipField::to_world(b.x, b.y, b.z);
	const Vector3 diff = to - from;

	// 並んでいる軸方向だけ長い直方体
	Reference<szg::StaticMeshInstance> link = worldRoot->instantiate<szg::StaticMeshInstance>(nullptr, "Cube.obj");
	link->transform_mut().set_translate((from + to) * 0.5f);
	link->transform_mut().set_scale(Vector3{
		std::max(std::abs(diff.x), kLinkThickness),
		kLinkThickness,
		std::max(std::abs(diff.z), kLinkThickness),
	});
	if (!link->get_materials().empty()) {
		link->get_materials()[0].color = CColorRGB::YELLOW;
	}
	links.emplace_back(link);
}
