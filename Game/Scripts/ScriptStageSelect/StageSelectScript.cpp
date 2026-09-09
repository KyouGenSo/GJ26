#include "StageSelectScript.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <numbers>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/Texture/TextureLibrary.h>
#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Module/World/Mesh/Primitive/Rect3d.h>
#include <Engine/Module/World/Mesh/Primitive/StringRectInstance.h>
#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Runtime/Clock/WorldClock.h>
#include <Engine/Runtime/Input/Input.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Runtime/Scene/SceneManager2.h>
#include <Engine/Runtime/Scene/World/WorldRoot.h>

#include <Engine/Assets/Json/JsonAsset.h>
#define COLOR_RGB_SERIALIZER
#include <Engine/Assets/Json/JsonSerializer.h>
#include <Engine/Assets/Json/Serializer/UnormSerializer.h>

#include <Library/Math/ColorRGB.h>
#include <Library/Math/Quaternion.h>

#include "Scripts/Scene/FactoryGJ26.h"

namespace {

constexpr r32 kFloorTextureGridSize = 5.0f;
// 画面外スロットの縮小率。0 だとワールド行列が非可逆になりエンジンが毎フレーム警告するので、見えない程度の正の値にする
constexpr r32 kHiddenPreviewScale = 0.001f;
/// セレクトで使う音。BGM はループ、決定音と戻る音はシーン遷移をまたいで鳴らす
constexpr std::array<string_literal, 4> kSounds{ "selectBgm.wav", "decision.wav", "choice.wav", "back.wav" };

r32 NearestEquivalentDegrees(r32 targetDegrees, r32 referenceDegrees) {
	return targetDegrees +
		std::round((referenceDegrees - targetDegrees) / 360.0f) * 360.0f;
}

r32 SmoothStep(r32 value) {
	const r32 t = std::clamp(value, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

r32 Lerp(r32 from, r32 to, r32 t) {
	return from + (to - from) * t;
}

} // namespace

void StageSelectScript::RegisterAudioAssets() {
	SoundPlayer::RegisterLoadQue(kSounds);
}

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
		leftArrowBaseScale = leftArrow->transform_imm().get_scale();
	}
	if (rightArrow) {
		rightArrowBasePosition = rightArrow->transform_imm().get_translate();
		rightArrowBaseScale = rightArrow->transform_imm().get_scale();
	}

	// 左右移動の入力を登録
	keys.initialize(
		{ szg::KeyID::A, szg::KeyID::D, szg::KeyID::Space, szg::KeyID::Escape },
		szg::InputInitializeMode::Current);
	pad.initialize({ szg::PadID::A, szg::PadID::Start }, szg::InputInitializeMode::Current);
	mouse.initialize({ szg::MouseID::Left }, szg::InputInitializeMode::Current);
	sound.initialize(kSounds);
	sound.play("selectBgm.wav");

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
	pad.update();
	mouse.update();
	const r32 deltaSeconds = szg::WorldClock::DeltaSeconds();
	update_arrow_animation(deltaSeconds);

	if (!sceneTransitionRequested && pad.trigger(szg::PadID::Start)) {
		sceneTransitionRequested = true;
		SoundPlayer::PlayAcrossScene("back.wav");
		szg::SceneManager2::SceneChange(SceneListGJ26::Title, 0.0f);
		return;
	}

	if (stageCount <= 0) {
		return;
	}

	if (!isTransitioning && !sceneTransitionRequested && pad.trigger(szg::PadID::A)) {
		sceneTransitionRequested = true;
		SoundPlayer::PlayAcrossScene("decision.wav");
		szg::RuntimeStorage::OverwirteValue("Temp", "StageNumber", i32{ selectedStage });
		szg::SceneManager2::SceneChange(SceneListGJ26::GamePlay, 0.0f);
		return;
	}

	const i32 currentStickDirection = stick_direction();
	// 決定後はカルーセルを動かさない(選択音も鳴らさない)
	if (!isTransitioning && !sceneTransitionRequested) {
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

	szg::JsonAsset parameter{ "[[game]]/StageSelect.param" };

	centerPreviewExtent = parameter.get().value("CenterPreviewExtent", nlohmann::json::object()).value("value", centerPreviewExtent);
	sidePreviewExtent = parameter.get().value("SidePreviewExtent", nlohmann::json::object()).value("value", sidePreviewExtent);
	transitionDuration = parameter.get().value("TransitionDuration", nlohmann::json::object()).value("value", transitionDuration);
	arrowAnimationPeriod = parameter.get().value("ArrowAnimationPeriod", nlohmann::json::object()).value("value", arrowAnimationPeriod);
	arrowMoveAmplitude = parameter.get().value("ArrowMoveAmplitude", nlohmann::json::object()).value("value", arrowMoveAmplitude);
	arrowReactionDuration = parameter.get().value("ArrowReactionDuration", nlohmann::json::object()).value("value", arrowReactionDuration);
	arrowReactionDistance = parameter.get().value("ArrowReactionDistance", nlohmann::json::object()).value("value", arrowReactionDistance);
	arrowReactionScale = parameter.get().value("ArrowReactionScale", nlohmann::json::object()).value("value", arrowReactionScale);
	arrowReactionOvershoot = parameter.get().value("ArrowReactionOvershoot", nlohmann::json::object()).value("value", arrowReactionOvershoot);
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
	Reference<szg::StaticMeshInstance> ground =
		worldRoot->instantiate<szg::StaticMeshInstance>(root, "Cube.obj");
	ground->transform_mut().set_scale(Vector3{
		static_cast<r32>(preview.field.width()),
		floorThickness,
		static_cast<r32>(preview.field.depth()),
	});
	ground->transform_mut().set_translate(Vector3{
		0.0f,
		-preview.field.center().y - (0.5f + floorThickness * 0.5f),
		0.0f,
	});
	if (!ground->get_materials().empty()) {
		ground->get_materials()[0].color = ColorRGB{ 0.3f, 0.3f, 0.3f };
		ground->get_materials()[0].texture = szg::TextureLibrary::GetTexture("floor.png");
		ground->get_materials()[0].uvTransform.set_scale(Vector2{
			static_cast<r32>(preview.field.depth()) / kFloorTextureGridSize,
			static_cast<r32>(preview.field.width()) / kFloorTextureGridSize,
		});
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

	sound.restart("choice.wav");

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
	arrowReactionDirection = step < 0 ? -1 : 1;
	arrowReactionElapsed = 0.0f;
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

		// 退場したプレビューはここで破棄する(枠は次の切り替えで再利用)。破棄後も次フレームまで描かれるが、scale は kHiddenPreviewScale なので見えない
		preview.field.destroy_root();
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
	r32 reactionOffset = 0.0f;
	r32 reactionScaleMultiplier = 1.0f;
	if (arrowReactionDirection != 0) {
		arrowReactionElapsed += deltaSeconds;
		const r32 duration = std::max(arrowReactionDuration, 0.001f);
		const r32 t = std::clamp(arrowReactionElapsed / duration, 0.0f, 1.0f);

		// 素早く外側へ飛び出し、基準位置側へ少し跳ね返ってから戻る。
		if (t < 0.35f) {
			const r32 eased = SmoothStep(t / 0.35f);
			reactionOffset = Lerp(0.0f, arrowReactionDistance, eased);
			reactionScaleMultiplier = 1.0f + arrowReactionScale * eased;
		}
		else if (t < 0.7f) {
			const r32 eased = SmoothStep((t - 0.35f) / 0.35f);
			reactionOffset = Lerp(arrowReactionDistance, -arrowReactionOvershoot, eased);
			reactionScaleMultiplier = Lerp(1.0f + arrowReactionScale, 1.0f - arrowReactionScale * 0.25f, eased);
		}
		else {
			const r32 eased = SmoothStep((t - 0.7f) / 0.3f);
			reactionOffset = Lerp(-arrowReactionOvershoot, 0.0f, eased);
			reactionScaleMultiplier = Lerp(1.0f - arrowReactionScale * 0.25f, 1.0f, eased);
		}

		if (t >= 1.0f) {
			arrowReactionDirection = 0;
			arrowReactionElapsed = 0.0f;
		}
	}

	if (leftArrow) {
		Vector3 position = leftArrowBasePosition;
		position.x -= offset;
		const r32 scale = arrowReactionDirection < 0 ? reactionScaleMultiplier : 1.0f;
		if (arrowReactionDirection < 0) {
			position.x -= reactionOffset;
		}
		leftArrow->transform_mut().set_translate(position);
		leftArrow->transform_mut().set_scale(Vector3{
			leftArrowBaseScale.x * scale,
			leftArrowBaseScale.y * scale,
			leftArrowBaseScale.z * scale,
		});
	}
	if (rightArrow) {
		Vector3 position = rightArrowBasePosition;
		position.x += offset;
		const r32 scale = arrowReactionDirection > 0 ? reactionScaleMultiplier : 1.0f;
		if (arrowReactionDirection > 0) {
			position.x += reactionOffset;
		}
		rightArrow->transform_mut().set_translate(position);
		rightArrow->transform_mut().set_scale(Vector3{
			rightArrowBaseScale.x * scale,
			rightArrowBaseScale.y * scale,
			rightArrowBaseScale.z * scale,
		});
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
		return kHiddenPreviewScale;
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
