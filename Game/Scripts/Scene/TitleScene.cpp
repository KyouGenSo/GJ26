#include "TitleScene.h"
#include <cmath>
#include <numbers>
#include <optional>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/PolygonMesh/PolygonMeshLibrary.h>
#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Module/World/Mesh/Primitive/Rect3d.h>
#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Module/World/Mesh/SkinningMeshInstance.h>
#include <Engine/Runtime/Clock/WorldClock.h>
#include <Engine/Runtime/Input/InputHandler.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Runtime/Scene/SceneManager2.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>
#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Library/Utility/Tools/SmartPointer.h>
#include "Scripts/Instance/FollowCamera/FollowCamera.h"
#include "Scripts/Instance/Skydome/Skydome.h"
#include "Scripts/Scene/FactoryGJ26.h"

namespace {

/// A ボタン UI が上下に 1 往復する秒数
constexpr r32 kStartButtonFloatPeriodSeconds = 2.0f;
/// A ボタン UI が基準位置から上下に動く最大距離
constexpr r32 kStartButtonFloatAmplitude = 0.1f;

/// <summary>
/// タイトルの開始入力と A ボタン UI の浮遊演出
/// </summary>
class TitleScript final : public szg::ISceneScript {
public:
	TitleScript() {
		pad_.initialize({ szg::PadID::A }, szg::InputInitializeMode::Current);
		mouse_.initialize({ szg::MouseID::Left }, szg::InputInitializeMode::Current);
	}
	~TitleScript() override = default;

	SZG_CLASS_MOVE_ONLY(TitleScript)

public:
	/// 浮遊させる A ボタン UI を受け取り、現在位置を基準位置として控える
	void set_start_button(Reference<szg::Rect3d> button) {
		startButton_ = button;
		if (startButton_) {
			startButtonBasePosition_ = startButton_->transform_imm().get_translate();
		}
	}

	void prev_update() override {
		update_start_button_float();

		pad_.update();
		mouse_.update();
		const bool startTriggered =
			pad_.trigger(szg::PadID::A) || mouse_.trigger(szg::MouseID::Left);
		if (transitionRequested_ || !startTriggered) {
			return;
		}

		transitionRequested_ = true;
		szg::SceneManager2::SceneChange(SceneListGJ26::Select, 0.0f);
	}

private:
	/// 基準位置を中心に sin で上下させる
	void update_start_button_float() {
		if (!startButton_) {
			return;
		}
		floatTime_ += szg::WorldClock::DeltaSeconds();
		const r32 phase = floatTime_ * (2.0f * std::numbers::pi_v<r32> / kStartButtonFloatPeriodSeconds);
		Vector3 position = startButtonBasePosition_;
		position.y += std::sin(phase) * kStartButtonFloatAmplitude;
		startButton_->transform_mut().set_translate(position);
	}

	szg::InputHandler<szg::PadID> pad_;
	szg::InputHandler<szg::MouseID> mouse_;
	bool transitionRequested_{ false };

	Reference<szg::Rect3d> startButton_;
	Vector3 startButtonBasePosition_{ CVector3::ZERO };
	r32 floatTime_{ 0.0f };
};

} // namespace

TitleScene::TitleScene() noexcept {
	set_name("Title");
}

void TitleScene::custom_load_asset() {
}

void TitleScene::custom_setup() {
	if (Reference<szg::WorldCluster> world = world_mut(0)) {
		world->world_root_mut().instantiate<Skydome>(nullptr);
	}
	else {
		szgError("Title: world 0 not found.");
	}

	auto cameraInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::CameraInstance>>("RuntimeInstance", "MainCamera");
	auto cameraFollowTargetInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::WorldInstance>>("RuntimeInstance", "CameraFollowTarget");

	const auto startButtonInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::Rect3d>>("RuntimeInstance", "StartButton");
	if (!startButtonInstance) {
		szgWarning("Title: StartButton runtime instance not found.");
	}

	if (cameraInstance && cameraFollowTargetInstance) {
		followCameraScript = eps::CreateUnique<FollowCamera>(cameraInstance.value_or(nullptr), cameraFollowTargetInstance.value_or(nullptr));
	}

	if (followCameraScript) {
		sceneScriptManager.register_script(std::move(followCameraScript));
	}
	std::unique_ptr<TitleScript> titleScript = eps::CreateUnique<TitleScript>();
	titleScript->set_start_button(startButtonInstance.value_or(nullptr));
	sceneScriptManager.register_script(std::move(titleScript));
}
