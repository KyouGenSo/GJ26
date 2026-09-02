#pragma once

#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Runtime/Scene/World/WorldRoot.h>
#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Library/Math/Quaternion.h>
#include <Library/Math/Vector3.h>
#include <Library/Utility/Template/Reference.h>

/// <summary>
/// ステージエディターシーンのオービットカメラ制御スクリプト
/// </summary>
class StageEditorCameraScript final : public szg::ISceneScript {
public:
	StageEditorCameraScript() = default;
	~StageEditorCameraScript() = default;

	SZG_CLASS_MOVE_ONLY(StageEditorCameraScript)

public:
	void setup(Reference<szg::WorldRoot> worldRoot_);
	void prev_update() override;

private:
	void initialize_camera();
	void update_view_point();

private:
	Reference<szg::WorldRoot> worldRoot;
	Reference<szg::CameraInstance> camera;

	Vector3 viewPoint{ 0.0f, 0.0f, 0.0f };
	Quaternion rotation{ CQuaternion::IDENTITY };
	r32 offset{ 15.0f };
	u32 lastDocVersion{ 0 };

	static constexpr r32 ROTATE_SENSITIVITY = 1.0f / 200.0f;
	static constexpr r32 PAN_SENSITIVITY = 1.0f / 100.0f;
	static constexpr r32 WHEEL_BASE_SPEED = 1.0f;
	static constexpr r32 WHEEL_SHIFT_MULTIPLIER = 10.0f;
	static constexpr r32 MIN_OFFSET = 0.1f;
};
