#include "StageSelectBackgroundEffect.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/Json/JsonAsset.h>
#include <Engine/Assets/PolygonMesh/PolygonMeshLibrary.h>
#include <Engine/Runtime/Clock/WorldClock.h>

std::array<StageSelectBackgroundEffect::ModelSetting, 6> StageSelectBackgroundEffect::DefaultModels() {
	return {{
		{ "goal", "[[game]]/goal/goal.obj", "goal.obj", 3.5f, 2, 0.45f, { 0.0f, 0.0f, 0.0f } },
		{ "ruler", "[[game]]/ruler/ruler.obj", "ruler.obj", 5.1f, 2, 0.18f, { 75.0f, 0.0f, 0.0f } },
		{ "drawingPaper", "[[game]]/drawingPaper/drawingPaper.obj", "drawingPaper.obj", 2.5f, 2, 0.4f, { 75.0f, 0.0f, 0.0f } },
		{ "scissors", "[[game]]/scissors/scissors.obj", "scissors.obj", 5.0f, 2, 0.23f, { 75.0f, 0.0f, 0.0f } },
		{ "thumbtack", "[[game]]/thumbtack/thumbtack.obj", "thumbtack.obj", 2.0f, 2, 0.65f, { 25.0f, 0.0f, 0.0f } },
		{ "goalPiece", "[[game]]/goalPiece/goalPiece.obj", "goalPiece.obj", 5.0f, 2, 0.3f, { 0.0f, 0.0f, 0.0f } },
	}};
}

void StageSelectBackgroundEffect::RegisterVisualAssets() {
	for (const auto& model : DefaultModels()) {
		szg::PolygonMeshLibrary::RegisterLoadQue(model.assetPath);
	}
}

void StageSelectBackgroundEffect::setup(
	Reference<szg::WorldRoot> world, Reference<szg::CameraInstance> camera) {
	finalize();
	camera_ = camera;
	if (!world || !camera_) {
		szgWarning("StageSelectBackgroundEffect: world or camera not found.");
		return;
	}
	setup_json_asset();
	camera_->update_affine();
	for (size_t index = 0; index < models_.size(); ++index) {
		const ModelSetting& model = models_[index];
		if (model.count == 0) {
			continue;
		}
		if (!szg::PolygonMeshLibrary::IsRegistered(model.meshName)) {
			szgWarning("StageSelectBackgroundEffect: {} is not loaded.", model.meshName);
			continue;
		}
		for (i32 i = 0; i < model.count; ++i) {
			FallingModel item;
			item.modelIndex = index;
			item.mesh = world->instantiate<szg::StaticMeshInstance>(nullptr, model.meshName);
			item.mesh->set_layer(0);
			item.mesh->transform_mut().set_scale(Vector3{ model.scale, model.scale, model.scale });
			respawn(item, true);
			apply_transform(item);
			items_.push_back(std::move(item));
		}
	}
	szgInformation("StageSelectBackgroundEffect: created {} models.", items_.size());
}

//==============================
// 更新処理
//==============================
void StageSelectBackgroundEffect::prev_update() {

	// カメラが存在しない、またはモデルが存在しない場合は更新しない
	if (!camera_ || items_.empty()) {
		return;
	}
	//カメラの更新
	camera_->update_affine();

	//deltaの取得
	const r32 delta = std::max(szg::WorldClock::DeltaSeconds(), 0.0f);

	// モデルの更新
	for (FallingModel& item : items_) {
		item.y -= item.speed * delta;
		item.phase = std::fmod(item.phase + delta * 2.0f * std::numbers::pi_v<r32> / swayPeriod_,
			2.0f * std::numbers::pi_v<r32>);
		item.rotationDegrees += item.angularVelocity * delta;
		for (size_t axis = 0; axis < 3; ++axis) {
			item.rotationDegrees[axis] = std::remainder(item.rotationDegrees[axis], 360.0f);
		}
		if (item.y < -1.0f - vertical_margin(item)) {
			respawn(item, false);
		}
		apply_transform(item);
	}
}

void StageSelectBackgroundEffect::finalize() {
	for (FallingModel& item : items_) {
		if (item.mesh && !item.mesh->is_marked_destroy()) {
			item.mesh->destroy_self();
		}
	}
	items_.clear();
	camera_.reset();
}

r32 StageSelectBackgroundEffect::random(r32 minimum, r32 maximum) {
	return std::uniform_real_distribution<r32>(minimum, maximum)(random_);
}

void StageSelectBackgroundEffect::respawn(FallingModel& item, bool initial) {
	item.x = random(-1.9f, 1.9f);
	item.y = initial ? random(-3.0f, 3.0f) : 1.0f + vertical_margin(item);
	item.speed = random(fallSpeedMin_, fallSpeedMax_);
	item.phase = random(0.0f, 2.0f * std::numbers::pi_v<r32>);
	item.rotationDegrees = models_[item.modelIndex].rotationDegrees;
	item.rotationDegrees.z += random(-180.0f, 180.0f);
	for (size_t axis = 0; axis < 3; ++axis) {
		const r32 sign = random(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
		item.angularVelocity[axis] = sign * random(rotationSpeedMin_, rotationSpeedMax_);
	}
}

r32 StageSelectBackgroundEffect::vertical_margin(const FallingModel& item) const {
	const auto& model = models_[item.modelIndex];
	const r32 radius = model.radius * model.scale;
	// 球の手前側が画面端に最も大きく投影される場合も含め、完全に画面外へ出す。
	return offscreenMargin_ + radius / cameraDepth_ *
		(1.0f + std::abs(camera_->proj_matrix()[1][1]));
}

void StageSelectBackgroundEffect::apply_transform(FallingModel& item) {
	const auto& model = models_[item.modelIndex];
	const r32 depth = cameraDepth_ + model.radius * model.scale;
	const auto& projection = camera_->proj_matrix();
	const r32 halfWidth = depth / std::max(std::abs(projection[0][0]), 0.001f);
	const r32 halfHeight = depth / std::max(std::abs(projection[1][1]), 0.001f);
	const Vector3 cameraLocal{
		(item.x + std::sin(item.phase) * swayAmplitude_) * halfWidth,
		item.y * halfHeight,
		depth,
	};
	item.mesh->transform_mut().set_translate(cameraLocal * camera_->world_affine());
	item.mesh->transform_mut().set_quaternion(Quaternion::EulerDegree(item.rotationDegrees));
	item.mesh->update_affine();
}

void StageSelectBackgroundEffect::setup_json_asset() {
	szg::JsonAsset parameter{ "[[game]]/StageSelectBackgroundEffect.param" };
	const auto& json = parameter.cget();
	if (!json.is_object()) {
		szgWarning("StageSelectBackgroundEffect: parameters not found; using defaults.");
		return;
	}
	const auto read = [&json](const std::string& name, r32 fallback) {
		const auto entry = json.find(name);
		if (entry == json.end() || !entry->is_object()) return fallback;
		const auto value = entry->find("value");
		if (value == entry->end() || !value->is_number()) return fallback;
		const r32 result = value->get<r32>();
		return std::isfinite(result) ? result : fallback;
	};
	fallSpeedMin_ = std::max(read("FallSpeedMin", fallSpeedMin_), 0.0f);
	fallSpeedMax_ = std::max(read("FallSpeedMax", fallSpeedMax_), fallSpeedMin_);
	rotationSpeedMin_ = std::max(read("RotationSpeedMin", rotationSpeedMin_), 0.0f);
	rotationSpeedMax_ = std::max(read("RotationSpeedMax", rotationSpeedMax_), rotationSpeedMin_);
	swayAmplitude_ = std::clamp(read("SwayAmplitude", swayAmplitude_), 0.0f, 0.5f);
	swayPeriod_ = std::max(read("SwayPeriod", swayPeriod_), 0.001f);
	// プレビューより奥、半径100のSkydomeより内側に収める。
	cameraDepth_ = std::clamp(read("CameraDepth", cameraDepth_), 14.0f, 50.0f);
	offscreenMargin_ = std::clamp(read("OffscreenMargin", offscreenMargin_), 0.0f, 0.5f);
	for (ModelSetting& model : models_) {
		const std::string name = model.name;
		model.count = static_cast<i32>(std::clamp(read(name + "Count", static_cast<r32>(model.count)), 0.0f, 64.0f));
		model.scale = std::clamp(read(name + "Scale", model.scale), 0.01f, 3.0f);
		const auto entry = json.find(name + "RotationDegrees");
		if (entry != json.end() && entry->is_object()) {
			const auto value = entry->find("value");
			if (value != entry->end() && value->is_object()) {
				const std::array<const char*, 3> axes{ "X", "Y", "Z" };
				for (size_t axis = 0; axis < axes.size(); ++axis) {
					const auto component = value->find(axes[axis]);
					if (component != value->end() && component->is_number()) {
						const r32 angle = component->get<r32>();
						if (std::isfinite(angle)) model.rotationDegrees[axis] = angle;
					}
				}
			}
		}
	}
}
