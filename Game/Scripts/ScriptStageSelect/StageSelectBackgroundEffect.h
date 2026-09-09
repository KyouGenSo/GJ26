#pragma once

#include <array>
#include <random>
#include <vector>

#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Runtime/Scene/World/WorldRoot.h>
#include <Engine/Runtime/SceneScript/ISceneScript.h>

/// セレクトのプレビューより奥で、文房具モデルを画面上から下へ循環させる。
class StageSelectBackgroundEffect final : public szg::ISceneScript {
public:
	static void RegisterVisualAssets();
	void setup(Reference<szg::WorldRoot> world, Reference<szg::CameraInstance> camera);
	void prev_update() override;
	void finalize() override;

private:
	struct ModelSetting {
		const char* name;
		const char* assetPath;
		const char* meshName;
		r32 radius; // 現在のモデルを原点中心で囲む球の半径(スケール適用前)
		i32 count;
		r32 scale;
		Vector3 rotationDegrees;
	};
	struct FallingModel {
		Reference<szg::StaticMeshInstance> mesh;
		size_t modelIndex{ 0 };
		r32 x{ 0.0f }, y{ 0.0f }; // 画面中央=0、端=±1
		r32 speed{ 0.0f }; //落下速度
		r32 phase{ 0.0f };//揺れの位相
		Vector3 rotationDegrees{};
		Vector3 angularVelocity{};
	};
	static std::array<ModelSetting, 6> DefaultModels();
	void setup_json_asset();
	void respawn(FallingModel& item, bool initial);
	void apply_transform(FallingModel& item);
	r32 vertical_margin(const FallingModel& item) const;
	r32 random(r32 minimum, r32 maximum);

	std::array<ModelSetting, 6> models_{ DefaultModels() };
	std::vector<FallingModel> items_;
	Reference<szg::CameraInstance> camera_;
	std::mt19937 random_{ std::random_device{}() };
	r32 fallSpeedMin_{ 0.10f }, fallSpeedMax_{ 0.18f }; // 画面半高さ/秒
	r32 rotationSpeedMin_{ 8.0f }, rotationSpeedMax_{ 20.0f }; // 度/秒
	r32 swayAmplitude_{ 0.04f }, swayPeriod_{ 3.5f };
	r32 cameraDepth_{ 18.0f }; // カメラからモデルの手前端までの距離
	r32 offscreenMargin_{ 0.15f }; // モデルの半径に加える画面外の余白
};
