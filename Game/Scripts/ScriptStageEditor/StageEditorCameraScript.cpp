#include "StageEditorCameraScript.h"

#include <algorithm>

#include <Engine/Application/Logger.h>
#include <Engine/Application/ProjectSettings/ProjectSettings.h>
#include <Engine/Module/World/Camera/ProjectionAdapter/CameraPerspectiveProjection.h>
#include <Engine/Runtime/Input/Input.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>

#include "Scripts/Editor/StageEditorDocument.h"
#include "Scripts/MapChip/MapChipField.h"

namespace {

/// <summary>
/// マウスホイールの値を正負1に正規化する
/// </summary>
r32 normalize_wheel(r32 wheel) {
	if (wheel > 0.0f) {
		return 1.0f;
	}
	if (wheel < 0.0f) {
		return -1.0f;
	}
	return 0.0f;
}

} // namespace

void StageEditorCameraScript::setup(Reference<szg::WorldRoot> worldRoot_) {
	worldRoot = worldRoot_;

	// Main.json の "Use runtime" カメラを取得
	std::optional<Reference<szg::CameraInstance>> cameraRef = szg::RuntimeStorage::GetValue<Reference<szg::CameraInstance>>("RuntimeInstance", "MainCamera");
	if (cameraRef) {
		camera = *cameraRef;
	}
	else {
		// フォールバック：新規カメラを作成
		camera = worldRoot->instantiate<szg::CameraInstance>(nullptr);
		auto projection = std::make_unique<szg::CameraPerspectiveProjection>();
		projection->initialize(
			0.45f,
			static_cast<r32>(szg::ProjectSettings::ClientWidth()) / szg::ProjectSettings::ClientHeight(),
			0.1f,
			10000.0f
		);
		camera->setup(std::move(projection));
	}

	initialize_camera();
}

void StageEditorCameraScript::prev_update() {
	if (!camera) {
		return;
	}

	StageEditorDocument& doc = StageEditorDocument::GetInstance();
	if (doc.version() != lastDocVersion) {
		lastDocVersion = doc.version();
		update_view_point();
	}

	const Vector2 mouseDelta = szg::Input::MouseDelta();
	const r32 wheel = normalize_wheel(szg::Input::WheelDelta());
	const bool isShift = szg::Input::IsPressKey(szg::KeyID::LShift);
	const bool isCtrl = szg::Input::IsPressKey(szg::KeyID::LControl);

	// マウスホイール：ズーム / 注視点移動
	if (wheel != 0.0f) {
		r32 wheelStep = wheel * WHEEL_BASE_SPEED;
		if (isShift) {
			wheelStep *= WHEEL_SHIFT_MULTIPLIER;
		}

		if (isCtrl) {
			// 注視点をカメラ前後方向に移動
			viewPoint += Vector3{ 0.0f, 0.0f, wheelStep } * rotation;
		}
		else {
			// 注視距離を変更
			offset = std::max(offset - wheelStep, MIN_OFFSET);
		}
	}

	// 右クリック：回転
	if (szg::Input::IsPressMouse(szg::MouseID::Right)) {
		const Vector2 rotateAngle = mouseDelta * ROTATE_SENSITIVITY;
		const Quaternion horizontal = Quaternion::AngleAxis(CVector3::BASIS_Y, rotateAngle.x);
		const Quaternion vertical = Quaternion::AngleAxis(CVector3::BASIS_X, rotateAngle.y);
		rotation = horizontal * rotation * vertical;
	}

	// カメラ姿勢を更新
	const Vector3 offsetVec = Vector3{ 0.0f, 0.0f, -offset } * rotation;
	camera->transform_mut().set_translate(viewPoint + offsetVec);
	camera->transform_mut().set_quaternion(rotation);
}

void StageEditorCameraScript::initialize_camera() {
	update_view_point();

	if (!camera) {
		return;
	}

	// 初期回転・距離を現在のカメラ姿勢から復元
	const Vector3 currentPosition = camera->transform_imm().get_translate();
	const Vector3 toCamera = currentPosition - viewPoint;
	offset = std::max(toCamera.length(), MIN_OFFSET);

	if (offset > MIN_OFFSET) {
		// カメラが viewPoint の方向を向く回転を計算
		const Vector3 forward = (viewPoint - currentPosition).normalize_safe();
		rotation = Quaternion::LookForward(forward, CVector3::BASIS_Y);
	}
	else {
		rotation = camera->transform_imm().get_quaternion();
	}

	const Vector3 offsetVec = Vector3{ 0.0f, 0.0f, -offset } * rotation;
	camera->transform_mut().set_translate(viewPoint + offsetVec);
	camera->transform_mut().set_quaternion(rotation);
}

void StageEditorCameraScript::update_view_point() {
	const StageEditorDocument& doc = StageEditorDocument::GetInstance();
	if (doc.width() > 0 && doc.depth() > 0) {
		const Vector3 center = MapChipField::to_world(doc.width() - 1, 0, doc.depth() - 1) * 0.5f;
		viewPoint = Vector3{ center.x, 0.0f, center.z };
	}
	else {
		viewPoint = Vector3{ 0.0f, 0.0f, 0.0f };
	}
}
