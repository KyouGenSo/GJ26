#include "BouncingTextVisual.h"

#include <cmath>

namespace {

constexpr r32 kPi = 3.14159265358979323846f;

}

void BouncingTextVisual::Create(Reference<szg::WorldCluster> world, const std::string& text, const Vector3& basePosition, r32 fontSize, u32 layer) {
	if (!world || text.empty()) {
		return;
	}

	basePosition_ = basePosition;
	bounceHeight_ = fontSize * kBounceHeightRatio;

	const r32 spacing = fontSize * kCharSpacingRatio;
	const int32_t charCount = static_cast<int32_t>(text.size());
	const r32 totalWidth = spacing * static_cast<r32>(charCount - 1);

	characters_.resize(charCount);
	characterOffsetsX_.resize(charCount);

	for (int32_t i = 0; i < charCount; ++i) {
		Reference<szg::StringRectInstance> character = world->world_root_mut().instantiate<szg::StringRectInstance>();
		if (!character) {
			continue;
		}

		character->initialize("UDEVGothic35HS-Regular.mtsdf", fontSize, CVector2::HALF);
		character->set_layer(layer); // ロード演出専用レイヤー(RenderPathでBaseリンクにより手前へ重ねる)
		character->set_draw(true);
		character->transform_mut().set_quaternion(CQuaternion::BACK_Y);
		character->set_blend_mode(szg::BlendMode::Alpha);

		auto& material = character->material_mut();
		material.lightingType = szg::LighingType::None;
		material.color = ColorRGBA{ 1.0f, 1.0f, 1.0f, 1.0f };

		character->reset_string(std::string(1, text[i]));

		characters_[i] = character;
		characterOffsetsX_[i] = -totalWidth * 0.5f + spacing * static_cast<r32>(i);
	}

	elapsed_ = 0.0f;
}

void BouncingTextVisual::Update(r32 deltaSeconds) {
	if (characters_.empty()) {
		return;
	}

	elapsed_ += deltaSeconds;

	for (size_t i = 0; i < characters_.size(); ++i) {
		if (!characters_[i]) {
			continue;
		}

		// 後方の文字ほど開始位相をkCharDelaySeconds分だけ遅らせ、波打つように見せる
		r32 charTime = std::fmod(elapsed_ - kCharDelaySeconds * static_cast<r32>(i), kBounceIntervalSeconds);
		if (charTime < 0.0f) {
			charTime += kBounceIntervalSeconds;
		}

		// 接地(0)→頂点→接地を1周期とするバウンド。|sin|は接地の瞬間に速度が滑らかに反転するため、
		// ボールが弾むような着地の間を再現できる
		const r32 phase = charTime / kBounceIntervalSeconds;
		const r32 bounceOffsetY = bounceHeight_ * std::fabs(std::sin(kPi * phase));

		Vector3 position = basePosition_;
		position.x += characterOffsetsX_[i];
		position.y += bounceOffsetY;
		position.z += kDepth;
		characters_[i]->transform_mut().set_translate(position);
		characters_[i]->transform_mut().set_quaternion(CQuaternion::BACK_Y);
	}
}

void BouncingTextVisual::SetVisible(bool isVisible) {
	for (auto& character : characters_) {
		if (character) {
			character->set_draw(isVisible);
		}
	}
}
