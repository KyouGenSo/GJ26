#include "FollowCamera.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <Engine/Runtime/Clock/WorldClock.h>

#include "Scripts/Instance/Player/Player.h"

FollowCamera::FollowCamera(
	Reference<szg::CameraInstance> cameraInstance,
	Reference<Player> owner) noexcept {
	set_camera_instance(cameraInstance);
	set_owner(owner);
}

void FollowCamera::finalize() {
	cameraInstance_.reset();
	owner_.reset();
	rotationInput_ = {};
	rotationDelta_ = {};
	shouldSnap_ = true;
}

void FollowCamera::prev_update() {
	if (!can_update()) {
		rotationInput_ = {};
		rotationDelta_ = {};
		return;
	}

	const float deltaSeconds = szg::WorldClock::DeltaSeconds();
	update_rotation(deltaSeconds);

	const Vector3 targetPosition = calculate_target_position();
	const Vector3 desiredPosition = calculate_desired_position(targetPosition);
	update_position(desiredPosition, deltaSeconds);
	cameraInstance_->look_at(targetPosition);
}

void FollowCamera::add_rotation_input(const Vector2& rotationInput) noexcept {
	rotationInput_ += rotationInput;
}

void FollowCamera::add_rotation_delta(const Vector2& rotationDelta) noexcept {
	rotationDelta_ += rotationDelta;
}

void FollowCamera::request_snap() noexcept {
	shouldSnap_ = true;
}

void FollowCamera::set_camera_instance(Reference<szg::CameraInstance> cameraInstance) noexcept {
	cameraInstance_ = cameraInstance;
	request_snap();
}

void FollowCamera::set_owner(Reference<Player> owner) noexcept {
	owner_ = owner;
	request_snap();
}

void FollowCamera::set_distance(float distance) noexcept {
	distance_ = std::max(distance, 0.1f);
}

void FollowCamera::set_rotation_speed(float rotationSpeed) noexcept {
	rotationSpeed_ = std::max(rotationSpeed, 0.0f);
}

void FollowCamera::set_follow_speed(float followSpeed) noexcept {
	followSpeed_ = std::max(followSpeed, 0.0f);
}

void FollowCamera::set_target_offset(const Vector3& targetOffset) noexcept {
	targetOffset_ = targetOffset;
}

void FollowCamera::set_rotation(float yaw, float pitch) noexcept {
	yaw_ = yaw;
	pitch_ = std::clamp(pitch, minPitch_, maxPitch_);
}

void FollowCamera::set_pitch_limits(float minPitch, float maxPitch) noexcept {
	if (minPitch > maxPitch) {
		std::swap(minPitch, maxPitch);
	}

	minPitch_ = minPitch;
	maxPitch_ = maxPitch;
	pitch_ = std::clamp(pitch_, minPitch_, maxPitch_);
}

Reference<szg::CameraInstance> FollowCamera::get_camera_instance_mut() noexcept {
	return cameraInstance_;
}

Reference<Player> FollowCamera::get_owner_mut() noexcept {
	return owner_;
}

float FollowCamera::get_distance() const noexcept {
	return distance_;
}

float FollowCamera::get_rotation_speed() const noexcept {
	return rotationSpeed_;
}

float FollowCamera::get_follow_speed() const noexcept {
	return followSpeed_;
}

const Vector3& FollowCamera::get_target_offset() const noexcept {
	return targetOffset_;
}

float FollowCamera::get_yaw() const noexcept {
	return yaw_;
}

float FollowCamera::get_pitch() const noexcept {
	return pitch_;
}

Vector3 FollowCamera::get_horizontal_forward() const noexcept {
	const Quaternion yawRotation = Quaternion::EulerRadian(0.0f, yaw_, 0.0f);
	return Vector3{ 0.0f, 0.0f, 1.0f } * yawRotation;
}

Vector3 FollowCamera::get_horizontal_right() const noexcept {
	const Quaternion yawRotation = Quaternion::EulerRadian(0.0f, yaw_, 0.0f);
	return Vector3{ 1.0f, 0.0f, 0.0f } * yawRotation;
}

bool FollowCamera::can_update() const noexcept {
	return cameraInstance_ && owner_ && owner_->get_world_instance_imm();
}

Vector3 FollowCamera::calculate_target_position() const noexcept {
	const auto ownerInstance = owner_->get_world_instance_imm();
	return ownerInstance->transform_imm().get_translate() + targetOffset_;
}

Vector3 FollowCamera::calculate_desired_position(const Vector3& targetPosition) const noexcept {
	const Quaternion rotation = Quaternion::EulerRadian(pitch_, yaw_, 0.0f);
	const Vector3 orbitOffset = Vector3{ 0.0f, 0.0f, -distance_ } * rotation;
	return targetPosition + orbitOffset;
}

void FollowCamera::update_rotation(float deltaSeconds) noexcept {
	const Vector2 rotation = rotationInput_ * deltaSeconds + rotationDelta_;
	yaw_ += rotation.x * rotationSpeed_;
	pitch_ += rotation.y * rotationSpeed_;
	pitch_ = std::clamp(pitch_, minPitch_, maxPitch_);
	yaw_ = std::remainder(yaw_, 2.0f * std::numbers::pi_v<float>);
	rotationInput_ = {};
	rotationDelta_ = {};
}

void FollowCamera::update_position(const Vector3& desiredPosition, float deltaSeconds) noexcept {
	auto& transform = cameraInstance_->transform_mut();
	if (shouldSnap_) {
		transform.set_translate(desiredPosition);
		shouldSnap_ = false;
		return;
	}

	const float interpolation = 1.0f - std::exp(-followSpeed_ * deltaSeconds);
	const Vector3 position = Vector3::Lerp(
		transform.get_translate(),
		desiredPosition,
		std::clamp(interpolation, 0.0f, 1.0f)
	);
	transform.set_translate(position);
}
