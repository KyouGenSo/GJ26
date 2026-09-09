#include "FadeOverlay.h"

#include <algorithm>

#include <Engine/GraphicsAPI/DirectX/DxResource/BufferObjects.h>
#include <Engine/Module/World/Mesh/Primitive/Rect3d.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>

void FadeOverlay::Create(Reference<szg::WorldCluster> world, const Vector2& size, u32 layer, const ColorRGB& color) {
	if (!world) {
		return;
	}

	overlay_ = world->world_root_mut().instantiate<szg::Rect3d>();
	if (!overlay_) {
		return;
	}

	overlay_->initialize(size, CVector2::HALF);
	overlay_->set_layer(layer);
	overlay_->set_draw(true);
	overlay_->set_blend_mode(szg::BlendMode::Alpha);

	auto& material = overlay_->material_mut();
	material.lightingType = szg::LighingType::None;
	material.color = ColorRGBA{ color.red, color.green, color.blue, currentAlpha_ };

	// kOverlayDepth は 3D World の「通常コンテンツ(Z=0 付近)」より手前に出すための既定値。
	// 直交投影 UI World のように別座標系で運用する場合は呼び出し側で transform を上書きすること。
	overlay_->transform_mut().set_translate(Vector3{ 0.0f, 0.0f, kOverlayDepth });
	overlay_->transform_mut().set_quaternion(CQuaternion::BACK_Y);
}

void FadeOverlay::FadeTo(r32 targetAlpha, r32 duration) {
	const r32 clampedDuration = std::max(duration, 0.0f);
	const r32 clampedTarget = std::clamp(targetAlpha, 0.0f, 1.0f);

	if (clampedDuration <= 0.0f) {
		SetAlphaImmediate(clampedTarget);
		return;
	}

	// 既に同じターゲットへ遷移中/到達済みの場合は何もしない(多重発行防止)
	if (std::abs(targetAlpha_ - clampedTarget) < 1e-4f && duration_ > 0.0f) {
		return;
	}

	startAlpha_ = currentAlpha_;
	targetAlpha_ = clampedTarget;
	duration_ = clampedDuration;
	elapsed_ = 0.0f;
}

void FadeOverlay::SetAlphaImmediate(r32 alpha) {
	currentAlpha_ = std::clamp(alpha, 0.0f, 1.0f);
	startAlpha_ = currentAlpha_;
	targetAlpha_ = currentAlpha_;
	duration_ = 0.0f;
	elapsed_ = 0.0f;

	if (overlay_) {
		auto& material = overlay_->material_mut();
		material.color.alpha = currentAlpha_;
	}
}

void FadeOverlay::SetTranslate(const Vector3& translate) {
	if (overlay_) {
		overlay_->transform_mut().set_translate(translate);
	}
}

void FadeOverlay::Update(r32 deltaSeconds) {
	if (duration_ <= 0.0f) {
		return;
	}

	// シーン遷移時の同期読み込みでフレームが極端に止まった場合、1フレームでアルファが
	// 一気に飛ぶのを防ぐ
	const r32 step = std::min(deltaSeconds, kMaxDeltaSeconds);
	elapsed_ += step;

	const r32 t = std::min(elapsed_ / duration_, 1.0f);
	currentAlpha_ = startAlpha_ + (targetAlpha_ - startAlpha_) * t;

	if (overlay_) {
		auto& material = overlay_->material_mut();
		material.color.alpha = currentAlpha_;
	}

	if (t >= 1.0f) {
		// 到達。以後は再要求が来るまで Update を早期 return させる
		duration_ = 0.0f;
	}
}

bool FadeOverlay::IsFading() const {
	return duration_ > 0.0f;
}