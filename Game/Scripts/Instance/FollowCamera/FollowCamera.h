#pragma once

#include <Engine/Module/World/Camera/CameraInstance.h>
#include <Engine/Runtime/SceneScript/ISceneScript.h>

#include <Library/Math/Vector2.h>
#include <Library/Math/Vector3.h>
#include <Library/Utility/Template/Reference.h>

class Player;

/// <summary>
/// Playerを中心に旋回する3人称視点カメラ
/// </summary>
class FollowCamera : public szg::ISceneScript {
public:
	FollowCamera() = default;
	FollowCamera(
		Reference<szg::CameraInstance> cameraInstance,
		Reference<Player> owner) noexcept;
	~FollowCamera() noexcept override = default;
	SZG_CLASS_MOVE_ONLY(FollowCamera)

public:
	void finalize() override;
	void prev_update() override;

public:
	/// 正規化された旋回入力を現在フレームへ加算する
	void add_rotation_input(const Vector2& rotationInput) noexcept;
	/// マウスなどのフレーム単位の旋回量を加算する
	void add_rotation_delta(const Vector2& rotationDelta) noexcept;
	/// 次回更新時に補間せず目的位置へ移動する
	void request_snap() noexcept;

	void set_camera_instance(Reference<szg::CameraInstance> cameraInstance) noexcept;
	void set_owner(Reference<Player> owner) noexcept;
	void set_distance(float distance) noexcept;
	void set_rotation_speed(float rotationSpeed) noexcept;
	void set_follow_speed(float followSpeed) noexcept;
	void set_target_offset(const Vector3& targetOffset) noexcept;
	void set_rotation(float yaw, float pitch) noexcept;
	void set_pitch_limits(float minPitch, float maxPitch) noexcept;

public:
	Reference<szg::CameraInstance> get_camera_instance_mut() noexcept;
	Reference<Player> get_owner_mut() noexcept;
	float get_distance() const noexcept;
	float get_rotation_speed() const noexcept;
	float get_follow_speed() const noexcept;
	const Vector3& get_target_offset() const noexcept;
	float get_yaw() const noexcept;
	float get_pitch() const noexcept;
	/// カメラ視点のXZ平面上の前方向を取得
	Vector3 get_horizontal_forward() const noexcept;
	/// カメラ視点のXZ平面上の右方向を取得
	Vector3 get_horizontal_right() const noexcept;

private:
	bool can_update() const noexcept;
	Vector3 calculate_target_position() const noexcept;
	Vector3 calculate_desired_position(const Vector3& targetPosition) const noexcept;
	void update_rotation(float deltaSeconds) noexcept;
	void update_position(const Vector3& desiredPosition, float deltaSeconds) noexcept;

private:
	Reference<szg::CameraInstance> cameraInstance_;
	Reference<Player> owner_;

	Vector2 rotationInput_;
	Vector2 rotationDelta_;
	Vector3 targetOffset_{ 0.0f, 1.0f, 0.0f };
	float distance_{ 10.0f };
	float rotationSpeed_{ 2.5f };
	float followSpeed_{ 10.0f };
	float yaw_{ 0.0f };
	float pitch_{ 0.3f };
	float minPitch_{ -1.2f };
	float maxPitch_{ 1.2f };
	bool shouldSnap_{ true };
};

