#include "GoalManager.h"

#include <algorithm>
#include <cmath>
#include <queue>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/Json/JsonAsset.h>
#include <Engine/Loader/EmitterInstanceLoader.h>
#include <Engine/Runtime/Particle/ParticlePool.h>
#include <Library/Math/ColorRGB.h>

#include "Scripts/Instance/Player/Player.h"

namespace {

constexpr r32 kLinkThickness = 0.12f;
// goalPiece.obj の星の中心(モデルローカル y ≈ 4.04)。モデルは 0.5 倍・底面基準で置かれる
constexpr r32 kPieceStarLocalY = 4.04f;
constexpr r32 kPieceModelScale = 0.5f;
constexpr r32 kLinkY = kPieceStarLocalY * kPieceModelScale - 0.5f;

} // namespace

void GoalManager::setup(Reference<MapChipField> field_, Reference<szg::WorldRoot> worldRoot_) {
	field = field_;
	worldRoot = worldRoot_;
	goalEffect.setup(field, worldRoot);

	szg::JsonAsset particle{ "[[game]]/GoalPieceEffect.particle" };
	pieceEmitterSettings = szg::EmitterInstanceLoader::Load(particle.cget());
	if (!pieceEmitterSettings) {
		szgWarning("GoalManager: GoalPieceEffect.particle could not be loaded.");
	}
}

void GoalManager::set_player(Reference<const Player> player_) {
	player = player_;
}

void GoalManager::finalize() {
	destroy_piece_emitters();
	goalEffect.finalize();
}

void GoalManager::post_update() {
	if (!field) {
		return;
	}
	// 動き始めたフレームで表示を消し、表示モデルの移動が終わってから再判定する
	if (field->version() != lastVersion) {
		lastVersion = field->version();
		destroy_links();
		destroy_piece_emitters();
		rebuildPending = true;
	}
	if (rebuildPending && !field->is_visual_interpolating()) {
		rebuildPending = false;
		rebuild();
	}
	goalEffect.update();

	// 操作対象は後から差し替えられるので毎フレーム取り直す
	const Reference<const szg::WorldInstance> instance = player ? player->get_world_instance_imm() : nullptr;
	const bool wasCleared = cleared;
	cleared = goalOpen && player && player->is_grounded() && instance && goal &&
		field->to_index(instance->world_position()) == *goal;
	if (cleared && !wasCleared) {
		szgInformation("GoalManager: stage clear");
	}
}

void GoalManager::rebuild() {
	destroy_links();
	destroy_piece_emitters();

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
	for (size_t i = 0; i < pieces.size(); ++i) {
		if (!adjacency[i].empty()) {
			create_piece_emitter(pieces[i]);
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
	}
	goalEffect.set_goal(goal, goalOpen);
}

bool GoalManager::is_connected(const MapChipIndex& a, const MapChipIndex& b) const {
	// 同じ高さで、X か Z のどちらか一方だけが揃っている。間の遮蔽は星のある上段(y+1)で見る
	if (a.y != b.y || (a.x == b.x) == (a.z == b.z)) {
		return false;
	}
	const bool alongX = a.z == b.z;
	const i32 begin = std::min(alongX ? a.x : a.z, alongX ? b.x : b.z);
	const i32 end = std::max(alongX ? a.x : a.z, alongX ? b.x : b.z);
	const i32 upperY = a.y + 1;
	for (i32 i = begin + 1; i < end; ++i) {
		const MapChipType chip = alongX ? field->get(i, upperY, a.z) : field->get(a.x, upperY, i);
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
	link->transform_mut().set_translate((from + to) * 0.5f + Vector3{ 0.0f, kLinkY, 0.0f });
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

void GoalManager::destroy_links() {
	for (auto& link : links) {
		if (link) {
			link->destroy_self();
		}
	}
	links.clear();
}

void GoalManager::create_piece_emitter(const MapChipIndex& index) {
	Reference<szg::StaticMeshInstance> visual = field ? field->visual_mut(index) : nullptr;
	if (!worldRoot || !visual || !pieceEmitterSettings) {
		return;
	}

	Reference<szg::EmitterInstance> emitter = worldRoot->instantiate<szg::EmitterInstance>(visual);
	emitter->setup_settings(*pieceEmitterSettings);
	Reference<szg::ParticlePool> pool = worldRoot->create_particle_pool(
		emitter,
		pieceEmitterSettings->capacity == 0 ? 1 : pieceEmitterSettings->capacity,
		pieceEmitterSettings->overflowPolicy);
	emitter->setup_pool(pool);
	if (pool) {
		pool->setup_draw_spec(pieceEmitterSettings->drawSpec);
		pool->setup_updaters(
			szg::EmitterInstance::BuildUpdaterMask(*pieceEmitterSettings),
			pieceEmitterSettings->rotation.rotationKind);
	}

	// 親モデルの縮小を打ち消してワールド等倍で放出する
	const r32 inverseScale = 1.0f / kPieceModelScale;
	emitter->transform_mut().set_translate(Vector3{ 0.0f, kPieceStarLocalY, 0.0f });
	emitter->transform_mut().set_scale(Vector3{ inverseScale, inverseScale, inverseScale });
	emitter->update_affine();
	emitter->restart_schedule();
	emitter->set_active(true);
	pieceEmitters.emplace_back(emitter);
}

void GoalManager::destroy_piece_emitters() {
	for (auto& emitter : pieceEmitters) {
		if (!emitter) {
			continue;
		}
		if (Reference<szg::ParticlePool> pool = emitter->pool_mut()) {
			pool->clear();
		}
		// 親のピースが残る場合に children へ無効参照を残さないよう先に外す
		emitter->reparent(nullptr, true);
		if (!emitter->is_marked_destroy()) {
			emitter->destroy_self();
		}
	}
	pieceEmitters.clear();
}
