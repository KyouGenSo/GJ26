#include "FollowCamera.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <Engine/Runtime/Clock/WorldClock.h>

FollowCamera::FollowCamera(
	Reference<szg::CameraInstance> cameraInstance,
	Reference<szg::WorldInstance> owner) noexcept {
	set_camera_instance(cameraInstance);
	set_owner(owner);
}

void FollowCamera::finalize() {
	cameraInstance_.reset();
	owner_.reset();
	goalEffect_.reset();
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
	if (goalEffect_) {
		rotationInput_ = {};
		rotationDelta_ = {};
		update_goal_effect(deltaSeconds);
		return;
	}
	update_rotation(deltaSeconds);

	const Vector3 targetPosition = calculate_target_position();
	const Vector3 desiredPosition = calculate_desired_position(targetPosition);
	update_position(desiredPosition, deltaSeconds);
	cameraInstance_->look_at(targetPosition);
}

void FollowCamera::add_rotation_input(const Vector2& rotationInput) noexcept {
	if (goalEffect_) {
		return;
	}
	rotationInput_ += rotationInput;
}

void FollowCamera::add_rotation_delta(const Vector2& rotationDelta) noexcept {
	if (goalEffect_) {
		return;
	}
	rotationDelta_ += rotationDelta;
}

void FollowCamera::request_snap() noexcept {
	shouldSnap_ = true;
}

void FollowCamera::set_camera_instance(Reference<szg::CameraInstance> cameraInstance) noexcept {
	cameraInstance_ = cameraInstance;
	request_snap();
}

void FollowCamera::set_owner(Reference<szg::WorldInstance> owner) noexcept {
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

void FollowCamera::fit_to_bounds(const Vector3& boundsSize, float padding) noexcept {
	const float radius = boundsSize.length() * 0.5f;
	if (radius <= 0.0f) {
		return;
	}

	// 射影行列の対角成分は各軸の半画角の cot。狭い方の画角に
	// バウンディング球を収め、旋回方向にかかわらずステージ全体を映す。
	float limitingCotangent = 1.0f;
	if (cameraInstance_) {
		const Matrix4x4& projection = cameraInstance_->proj_matrix();
		limitingCotangent = std::max(
			std::abs(projection[0][0]),
			std::abs(projection[1][1]));
	}
	const float safePadding = std::max(padding, 1.0f);
	set_distance(radius * std::sqrt(limitingCotangent * limitingCotangent + 1.0f) * safePadding);
	request_snap();
}

bool FollowCamera::start_goal_effect(
	const Vector3& targetPosition,
	float duration,
	float distance,
	float elevationDegrees,
	float bounceStrength) noexcept {
	if (!cameraInstance_ || !owner_) {
		return false;
	}

	const Vector3 cameraPosition = cameraInstance_->transform_imm().get_translate();
	Vector3 horizontalDirection{
		cameraPosition.x - targetPosition.x,
		0.0f,
		cameraPosition.z - targetPosition.z,
	};
	float horizontalLength = std::sqrt(
		horizontalDirection.x * horizontalDirection.x +
		horizontalDirection.z * horizontalDirection.z);
	if (horizontalLength <= 0.001f) {
		const Vector3 fallback = Vector3{ 0.0f, 0.0f, -1.0f } *
			Quaternion::EulerRadian(0.0f, yaw_, 0.0f);
		horizontalDirection = { fallback.x, 0.0f, fallback.z };
		horizontalLength = std::max(std::sqrt(
			horizontalDirection.x * horizontalDirection.x +
			horizontalDirection.z * horizontalDirection.z), 0.001f);
	}
	horizontalDirection /= horizontalLength;

	const float safeDistance = std::max(distance, 0.1f);
	const float elevation = std::clamp(elevationDegrees, 0.0f, 89.0f) *
		(std::numbers::pi_v<float> / 180.0f);
	const float horizontalDistance = std::cos(elevation) * safeDistance;
	const float height = std::sin(elevation) * safeDistance;
	const Vector3 destination = targetPosition +
		horizontalDirection * horizontalDistance + Vector3{ 0.0f, height, 0.0f };

	goalEffect_ = GoalCameraEffect{
		.startPosition = cameraPosition,
		.startTarget = calculate_target_position(),
		.targetPosition = targetPosition,
		.destinationPosition = destination,
		.elapsed = 0.0f,
		.duration = std::max(duration, 0.001f),
		.bounceStrength = std::max(bounceStrength, 0.0f),
		.finished = false,
	};
	rotationInput_ = {};
	rotationDelta_ = {};
	shouldSnap_ = false;
	return true;
}

void FollowCamera::stop_goal_effect() noexcept {
	goalEffect_.reset();
	rotationInput_ = {};
	rotationDelta_ = {};
	request_snap();
}

bool FollowCamera::is_goal_effect_finished() const noexcept {
	return goalEffect_ && goalEffect_->finished;
}

Reference<szg::CameraInstance> FollowCamera::get_camera_instance_mut() noexcept {
	return cameraInstance_;
}

Reference<szg::WorldInstance> FollowCamera::get_owner_mut() noexcept {
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
	return cameraInstance_ && owner_;
}

Vector3 FollowCamera::calculate_target_position() const noexcept {
	const auto ownerInstance = owner_;
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

void FollowCamera::update_goal_effect(float deltaSeconds) noexcept {
	if (!cameraInstance_ || !goalEffect_) {
		return;
	}

	GoalCameraEffect& effect = *goalEffect_;
	if (effect.finished) {
		cameraInstance_->transform_mut().set_translate(effect.destinationPosition);
		cameraInstance_->look_at(effect.targetPosition);
		return;
	}

	effect.elapsed += deltaSeconds;
	const float t = std::clamp(effect.elapsed / effect.duration, 0.0f, 1.0f);
	const float smoothTargetT = t * t * (3.0f - 2.0f * t);
	const float shifted = t - 1.0f;
	// 1.0を一度越えて戻るEaseOutBack。カメラが近づき過ぎてから最終距離へ戻る。
	const float cameraT = 1.0f +
		(effect.bounceStrength + 1.0f) * shifted * shifted * shifted +
		effect.bounceStrength * shifted * shifted;

	const Vector3 cameraPosition = Vector3::Lerp(
		effect.startPosition,
		effect.destinationPosition,
		cameraT);
	const Vector3 lookTarget = Vector3::Lerp(
		effect.startTarget,
		effect.targetPosition,
		smoothTargetT);
	cameraInstance_->transform_mut().set_translate(cameraPosition);
	cameraInstance_->look_at(lookTarget);

	if (t >= 1.0f) {
		effect.finished = true;
	}
}
