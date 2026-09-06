#include "TitleScene.h"
#include <optional>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/PolygonMesh/PolygonMeshLibrary.h>
#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Module/World/Mesh/SkinningMeshInstance.h>
#include <Engine/Runtime/Input/InputHandler.h>
#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Runtime/Scene/SceneManager2.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>
#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Library/Utility/Tools/SmartPointer.h>
#include "Scripts/Instance/FollowCamera/FollowCamera.h"
#include "Scripts/Scene/FactoryGJ26.h"

namespace {

class TitleInputScript final : public szg::ISceneScript {
public:
	TitleInputScript() {
		pad_.initialize({ szg::PadID::A }, szg::InputInitializeMode::Current);
		mouse_.initialize({ szg::MouseID::Left }, szg::InputInitializeMode::Current);
	}

	void prev_update() override {
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
	szg::InputHandler<szg::PadID> pad_;
	szg::InputHandler<szg::MouseID> mouse_;
	bool transitionRequested_{ false };
};

} // namespace

TitleScene::TitleScene() noexcept {
	set_name("Title");
}

void TitleScene::custom_load_asset() {
}

void TitleScene::custom_setup() {

	auto cameraInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::CameraInstance>>("RuntimeInstance", "MainCamera");
	auto cameraFollowTargetInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::WorldInstance>>("RuntimeInstance", "CameraFollowTarget");

	auto titleTextInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::StringRectInstance>>("RuntimeInstance", "TitleText");
	auto startButtonTextInstance =
		szg::RuntimeStorage::GetValue<Reference<szg::StringRectInstance>>("RuntimeInstance", "StartButtonText");

	if (cameraInstance && cameraFollowTargetInstance) {
		followCameraScript = eps::CreateUnique<FollowCamera>(cameraInstance.value_or(nullptr), cameraFollowTargetInstance.value_or(nullptr));
	}

	if (followCameraScript) {
		sceneScriptManager.register_script(std::move(followCameraScript));
	}
	sceneScriptManager.register_script(eps::CreateUnique<TitleInputScript>());
}
