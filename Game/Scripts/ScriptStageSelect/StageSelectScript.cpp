#include "StageSelectScript.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

#include <Engine/Application/Logger.h>
#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Module/World/Mesh/Primitive/Rect3d.h>
#include <Engine/Module/World/Mesh/Primitive/StringRectInstance.h>
#include <Engine/Runtime/Clock/WorldClock.h>
#include <Engine/Runtime/Input/Input.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Runtime/Scene/World/WorldRoot.h>

#include <Engine/Assets/Json/JsonAsset.h>
#define COLOR_RGB_SERIALIZER
#include <Engine/Assets/Json/JsonSerializer.h>
#include <Engine/Assets/Json/Serializer/UnormSerializer.h>

#include <Library/Math/ColorRGB.h>
#include <Library/Math/Quaternion.h>

namespace {

r32 NearestEquivalentDegrees(r32 targetDegrees, r32 referenceDegrees) {
	return targetDegrees +
		std::round((referenceDegrees - targetDegrees) / 360.0f) * 360.0f;
}

} // namespace

void StageSelectScript::setup(
	Reference<szg::WorldRoot> worldRoot_,
	Reference<szg::CameraInstance> previewCamera_,
	Reference<szg::StringRectInstance> stageNumberText_,
	Reference<szg::Rect3d> leftArrow_,
	Reference<szg::Rect3d> rightArrow_) {

	worldRoot = worldRoot_;
	previewCamera = previewCamera_;
	stageNumberText = stageNumberText_;
	leftArrow = leftArrow_;
	rightArrow = rightArrow_;
	if (leftArrow) {
		leftArrowBasePosition = leftArrow->transform_imm().get_translate();
	}
	if (rightArrow) {
		rightArrowBasePosition = rightArrow->transform_imm().get_translate();
	}

	// 左右移動の入力を登録
	keys.initialize({ szg::KeyID::A, szg::KeyID::D }, szg::InputInitializeMode::Current);

	// ステージの総数を取得
	stageCount = MapChipField::CountStages();
	if (stageCount <= 0) {
		szgWarning("SelectScene: '{}' not found.", MapChipField::StageDirectory(1));
		set_navigation_active(false, false);
		if (stageNumberText) {
			stageNumberText->reset_string("NO STAGE");
		}
		return;
	}

	// 前回の選択ステージ番号を復元
	selectedStage = std::clamp(
		szg::RuntimeStorage::GetValue<i32>("Temp", "StageNumber").value_or(1),
		1,
		stageCount);
	selectedCarouselIndex = selectedStage;

	// jsonAssetのセットアップ
	setup_json_asset();
	initialize_previews();
	update_selection_display();
}

void StageSelectScript::prev_update() {
	keys.update();
	const r32 deltaSeconds = szg::WorldClock::DeltaSeconds();
	update_arrow_animation(deltaSeconds);
	if (stageCount <= 0) {
		return;
	}

	const i32 currentStickDirection = stick_direction();
	if (!isTransitioning) {
		i32 step = 0;
		if (keys.trigger(szg::KeyID::A)) {
			step = -1;
		}
		else if (keys.trigger(szg::KeyID::D)) {
			step = 1;
		}
		else if (currentStickDirection != 0 && currentStickDirection != previousStickDirection) {
			step = currentStickDirection;
		}
		if (step != 0) {
			begin_transition(step);
		}
	}
	previousStickDirection = currentStickDirection;

	if (isTransitioning) {
		update_transition(deltaSeconds);
	}
	else {
		update_selected_rotation(deltaSeconds);
	}
	update_preview_float_animation(deltaSeconds);
}

//============================================================================
// jsonAssetのセットアップ
//============================================================================
void StageSelectScript::setup_json_asset() {

	szg::JsonAsset parameter{ "[[game]]/StageSelect.param", "param" };

	centerPreviewExtent = parameter.get().value("CenterPreviewExtent", nlohmann::json::object()).value("value", centerPreviewExtent);
	sidePreviewExtent = parameter.get().value("SidePreviewExtent", nlohmann::json::object()).value("value", sidePreviewExtent);
	transitionDuration = parameter.get().value("TransitionDuration", nlohmann::json::object()).value("value", transitionDuration);
	arrowAnimationPeriod = parameter.get().value("ArrowAnimationPeriod", nlohmann::json::object()).value("value", arrowAnimationPeriod);
	arrowMoveAmplitude = parameter.get().value("ArrowMoveAmplitude", nlohmann::json::object()).value("value", arrowMoveAmplitude);
	previewFloatAnimationPeriod = parameter.get().value("PreviewFloatAnimationPeriod", nlohmann::json::object()).value("value", previewFloatAnimationPeriod);
	previewFloatAmplitude = parameter.get().value("PreviewFloatAmplitude", nlohmann::json::object()).value("value", previewFloatAmplitude);
	transitionRotationDegrees = parameter.get().value("TransitionRotationDegrees", nlohmann::json::object()).value("value", transitionRotationDegrees);
}

//============================================================================
// 選択中のステージと、その前後のステージを初期配置する
//============================================================================
void StageSelectScript::initialize_previews() {
	if (stageCount == 1) {
		build_preview(previews[0], selectedCarouselIndex, 0);
		return;
	}

	for (i32 relativeSlot = -1; relativeSlot <= 1; ++relativeSlot) {
		const size_t previewIndex = static_cast<size_t>(relativeSlot + 1);
		build_preview(previews[previewIndex], selectedCarouselIndex + relativeSlot, relativeSlot);
	}
}

bool StageSelectScript::build_preview(Preview& preview, i64 carouselIndex, i32 relativeSlot) {
	const i32 stageNumber = wrapped_stage_number(carouselIndex);
	if (!preview.field.load_stage(stageNumber)) {
		return false;
	}
	preview.field.build(*worldRoot);
	Reference<szg::WorldInstance> root = preview.field.root_mut();
	if (!root) {
		return false;
	}

	// MapChipの最下層直下に、ステージと同じ親Transformで動く床を置く。
	Reference<szg::StaticMeshInstance> floor =
		worldRoot->instantiate<szg::StaticMeshInstance>(root, "Cube.obj");
	floor->transform_mut().set_scale(Vector3{
		static_cast<r32>(preview.field.width()),
		floorThickness,
		static_cast<r32>(preview.field.depth()),
	});
	floor->transform_mut().set_translate(Vector3{
		0.0f,
		-preview.field.center().y - (0.5f + floorThickness * 0.5f),
		0.0f,
	});
	if (!floor->get_materials().empty()) {
		floor->get_materials()[0].color = ColorRGB{ 0.3f, 0.3f, 0.3f };
	}

	preview.stageNumber = stageNumber;
	preview.carouselIndex = carouselIndex;
	preview.yawDegrees = slot_yaw(relativeSlot);
	preview.isUsed = true;
	const r32 x = slot_position_x(relativeSlot);
	const r32 scale = preview_scale(preview, relativeSlot);
	preview.startX = preview.targetX = x;
	preview.startScale = preview.targetScale = scale;
	preview.startYaw = preview.targetYaw = preview.yawDegrees;
	root->transform_mut().set_scale(Vector3{ scale, scale, scale });
	root->transform_mut().set_quaternion(
		Quaternion::EulerDegree(Vector3{ previewPitchDegrees, preview.yawDegrees, 0.0f }));
	root->transform_mut().set_translate(Vector3{ x, previewY, 0.0f });
	root->set_active(true);
	return true;
}

bool StageSelectScript::begin_transition(i32 step) {
	if (stageCount <= 1) {
		return false;
	}
	const i64 nextCarouselIndex = selectedCarouselIndex + step;

	// 移動先のさらに隣を、画面外の予備スロットへ先に生成する。
	const i64 incomingCarouselIndex = nextCarouselIndex + step;
	if (!find_preview(incomingCarouselIndex)) {
		Preview* incoming = find_unused_preview();
		if (incoming) {
			build_preview(*incoming, incomingCarouselIndex, step * 2);
		}
	}

	// 選択中のステージのインデックスを更新
	selectedCarouselIndex = nextCarouselIndex;
	selectedStage = wrapped_stage_number(selectedCarouselIndex);
	for (Preview& preview : previews) {
		if (!preview.isUsed) {
			continue;
		}
		Reference<szg::WorldInstance> root = preview.field.root_mut();
		if (!root) {
			continue;
		}

		const i32 relativeSlot = static_cast<i32>(preview.carouselIndex - selectedCarouselIndex);

		// 移動前の位置とスケールを保存して、移動後の位置とスケールを計算する
		preview.startX = root->transform_imm().get_translate().x;
		preview.startScale = root->transform_imm().get_scale().x;
		preview.startYaw = preview.yawDegrees;
		preview.targetX = slot_position_x(relativeSlot);
		preview.targetScale = preview_scale(preview, relativeSlot);
		preview.targetYaw = NearestEquivalentDegrees(slot_yaw(relativeSlot), preview.startYaw);
	}

	transitionElapsed = 0.0f;
	isTransitioning = true;
	update_selection_display();
	return true;
}

void StageSelectScript::update_transition(r32 deltaSeconds) {
	if (!isTransitioning) {
		return;
	}

	transitionElapsed += deltaSeconds;
	const r32 duration = std::max(transitionDuration, 0.001f);
	const r32 t = std::clamp(transitionElapsed / duration, 0.0f, 1.0f);
	const r32 eased = 1.0f - std::pow(1.0f - t, 3.0f);
	for (Preview& preview : previews) {
		if (!preview.isUsed) {
			continue;
		}
		Reference<szg::WorldInstance> root = preview.field.root_mut();
		if (!root) {
			continue;
		}
		const r32 x = preview.startX + (preview.targetX - preview.startX) * eased;
		const r32 scale = preview.startScale + (preview.targetScale - preview.startScale) * eased;
		preview.yawDegrees = preview.startYaw + (preview.targetYaw - preview.startYaw) * eased;
		root->transform_mut().set_translate(Vector3{ x, previewY, 0.0f });
		root->transform_mut().set_scale(Vector3{ scale, scale, scale });
		root->transform_mut().set_quaternion(
			Quaternion::EulerDegree(Vector3{ previewPitchDegrees, preview.yawDegrees, 0.0f }));
	}

	if (t >= 1.0f) {
		finish_transition();
	}
}

void StageSelectScript::finish_transition() {
	for (Preview& preview : previews) {
		if (!preview.isUsed) {
			continue;
		}
		if (std::abs(preview.carouselIndex - selectedCarouselIndex) <= 1) {
			continue;
		}

		// MapChipFieldは次にこの予備枠を再利用するときrootごと破棄する。
		if (Reference<szg::WorldInstance> root = preview.field.root_mut()) {
			root->transform_mut().set_scale(CVector3::ZERO);
		}
		preview.isUsed = false;
	}
	isTransitioning = false;
}


//============================================================================
// 選択中のステージのプレビューを回転させる処理
//============================================================================
void StageSelectScript::update_selected_rotation(r32 deltaSeconds) {
	Preview* selected = find_preview(selectedCarouselIndex);
	if (!selected) {
		return;
	}
	Reference<szg::WorldInstance> root = selected->field.root_mut();
	if (!root) {
		return;
	}

	selected->yawDegrees += selectedRotationSpeedDegrees * deltaSeconds;
	if (selected->yawDegrees >= 360.0f) {
		selected->yawDegrees -= 360.0f;
	}
	root->transform_mut().set_quaternion(
		Quaternion::EulerDegree(Vector3{ previewPitchDegrees, selected->yawDegrees, 0.0f }));
}

//============================================================================
// 矢印を左右対称にゆっくり往復させる
//============================================================================
void StageSelectScript::update_arrow_animation(r32 deltaSeconds) {
	arrowAnimationTime += deltaSeconds;
	const r32 period = std::max(arrowAnimationPeriod, 0.001f);
	const r32 phase = arrowAnimationTime * (2.0f * std::numbers::pi_v<r32> / period);
	const r32 offset = std::sin(phase) * arrowMoveAmplitude;

	if (leftArrow) {
		Vector3 position = leftArrowBasePosition;
		position.x -= offset;
		leftArrow->transform_mut().set_translate(position);
	}
	if (rightArrow) {
		Vector3 position = rightArrowBasePosition;
		position.x += offset;
		rightArrow->transform_mut().set_translate(position);
	}
}

//============================================================================
// ミニチュアモデルを上下にゆっくり往復させる
//============================================================================
void StageSelectScript::update_preview_float_animation(r32 deltaSeconds) {
	previewFloatAnimationTime += deltaSeconds;
	const r32 period = std::max(previewFloatAnimationPeriod, 0.001f);
	const r32 phase = previewFloatAnimationTime * (2.0f * std::numbers::pi_v<r32> / period);
	const r32 offset = std::sin(phase) * previewFloatAmplitude;

	for (Preview& preview : previews) {
		if (!preview.isUsed) {
			continue;
		}
		Reference<szg::WorldInstance> root = preview.field.root_mut();
		if (!root) {
			continue;
		}

		Vector3 position = root->transform_imm().get_translate();
		position.y = previewY + offset;
		root->transform_mut().set_translate(position);
	}
}

void StageSelectScript::update_selection_display() {
	szg::RuntimeStorage::OverwirteValue("Temp", "StageNumber", i32{ selectedStage });
	if (stageNumberText) {
		stageNumberText->reset_string(std::format("STAGE {}", selectedStage));
	}
	set_navigation_active(stageCount > 1, stageCount > 1);
}

StageSelectScript::Preview* StageSelectScript::find_preview(i64 carouselIndex) {
	for (Preview& preview : previews) {
		if (preview.isUsed && preview.carouselIndex == carouselIndex) {
			return &preview;
		}
	}
	return nullptr;
}

StageSelectScript::Preview* StageSelectScript::find_unused_preview() {
	for (Preview& preview : previews) {
		if (!preview.isUsed) {
			return &preview;
		}
	}
	return nullptr;
}

r32 StageSelectScript::preview_scale(const Preview& preview, i32 relativeSlot) const {
	const r32 maxDimension = static_cast<r32>(std::max({
		preview.field.width(),
		preview.field.height(),
		preview.field.depth(),
	}));
	if (maxDimension <= 0.0f || std::abs(relativeSlot) > 1) {
		return 0.0f;
	}
	const r32 targetExtent = relativeSlot == 0 ? centerPreviewExtent : sidePreviewExtent;
	return targetExtent / maxDimension;
}

r32 StageSelectScript::slot_position_x(i32 relativeSlot) const {
	return static_cast<r32>(relativeSlot) * slotSpacing;
}

r32 StageSelectScript::slot_yaw(i32 relativeSlot) const {
	const r32 centerYaw = arrival_yaw();
	const r32 rotationDegrees = std::abs(transitionRotationDegrees);
	if (relativeSlot < 0) {
		return centerYaw + rotationDegrees;
	}
	if (relativeSlot > 0) {
		return centerYaw - rotationDegrees;
	}
	return centerYaw;
}

//============================================================================
// 選択中のステージのプレビューのローカル-Zを
// カメラへ向けるためのYaw角度を取得する
//============================================================================
r32 StageSelectScript::arrival_yaw() const {
	if (!previewCamera) {
		return 0.0f;
	}

	// ローカル+Zをカメラと反対へ向けることで、ローカル-Zをカメラへ向ける。
	const Vector3 previewCenter{ 0.0f, previewY, 0.0f };
	Vector3 awayFromCamera = previewCamera->transform_imm().get_translate() - previewCenter;
	awayFromCamera.y = 0.0f;
	if (std::abs(awayFromCamera.x) < 0.0001f && std::abs(awayFromCamera.z) < 0.0001f) {
		return 0.0f;
	}
	return std::atan2(awayFromCamera.x, awayFromCamera.z) *
		(180.0f / std::numbers::pi_v<r32>);
}

i32 StageSelectScript::wrapped_stage_number(i64 carouselIndex) const {
	if (stageCount <= 0) {
		return 0;
	}
	i64 zeroBased = (carouselIndex - 1) % stageCount;
	if (zeroBased < 0) {
		zeroBased += stageCount;
	}
	return static_cast<i32>(zeroBased) + 1;
}


//============================================================================
// 選択中のステージの前後のステージへ移動する
// 矢印の表示/非表示を切り替える
//============================================================================
void StageSelectScript::set_navigation_active(bool hasPrevious, bool hasNext) {
	if (leftArrow) {
		leftArrow->set_active(hasPrevious);
	}
	if (rightArrow) {
		rightArrow->set_active(hasNext);
	}
}

//============================================================================
// スティックの方向を取得する
//============================================================================
i32 StageSelectScript::stick_direction() const {
	const r32 stickX = szg::Input::StickL().x;
	if (stickX <= -stickThreshold) {
		return -1;
	}
	if (stickX >= stickThreshold) {
		return 1;
	}
	return 0;
}
