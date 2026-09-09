#include "ClayStretchAnimator.h"

#include <algorithm>
#include <cmath>

#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Runtime/Scene/World/WorldRoot.h>
#include <Engine/Module/World/WorldInstance/WorldInstance.h>

#include "ClayStretchMeshGenerator.h"
#include "MapChipField.h"

#include <Library/Utility/Tools/Easing.h>

r32 ClayStretchAnimator::compute_stretch_progress(r32 normalizedTime, const ClayStretchParams& params) const {
	const r32 squashEnd = params.squashRatio;
	const r32 pullEnd = params.squashRatio + params.pullRatio;

	if (normalizedTime < squashEnd) {
		return 0.0f;
	}
	if (normalizedTime < pullEnd) {
		const r32 t = (normalizedTime - squashEnd) / params.pullRatio;
		return Easing::Out::Quad(t) * params.pullProgress;
	}
	const r32 t = (normalizedTime - pullEnd) / params.settleRatio;
	return params.pullProgress + Easing::Out::Back(t) * (1.0f - params.pullProgress);
}

i32 ClayStretchAnimator::select_keyframe(r32 progress) const {
	if (progress <= 0.001f) {
		return 0;
	}
	const i32 maxFrame = cachedParams.keyframeCount - 1;
	const r32 scaled = progress * static_cast<r32>(maxFrame);
	return std::min(static_cast<i32>(std::ceil(scaled)), maxFrame);
}

void ClayStretchAnimator::begin(
	i32 originFlat_,
	i32 fromFlat_,
	i32 toFlat_,
	const Vector3& direction_,
	r32 duration_,
	MapChipField& field,
	szg::WorldRoot& worldRoot,
	Reference<szg::WorldInstance> root,
	Reference<szg::StaticMeshInstance> baseVisual_) {

	if (active) {
		finish(field);
	}
	if (!root || !baseVisual_) {
		return;
	}

	if (!paramsLoaded) {
		cachedParams = ClayStretchMeshGenerator::LoadParams();
		paramsLoaded = true;
	}

	const i32 dirIndex = ClayStretchMeshGenerator::DirectionIndex(direction_);

	std::vector<std::string> meshNames;
	meshNames.reserve(cachedParams.keyframeCount);
	for (i32 frame = 0; frame < cachedParams.keyframeCount; ++frame) {
		meshNames.push_back(ClayStretchMeshGenerator::MeshName(dirIndex, frame));
	}

	StretchAnimation anim;
	anim.originFlat = originFlat_;
	anim.fromFlat = fromFlat_;
	anim.toFlat = toFlat_;
	anim.direction = direction_;
	anim.elapsed = 0.0f;
	anim.duration = std::max(duration_, 0.001f);
	anim.keyframeMeshNames = std::move(meshNames);
	anim.currentFrame = 0;
	anim.baseVisual = baseVisual_;

	const std::string& initialMeshName = anim.keyframeMeshNames[0];
	anim.capVisual = worldRoot.instantiate<szg::StaticMeshInstance>(root, initialMeshName);

	// 掴んでいるブロック(= FROM セル)の面から伸び始める
	// fromFlat_ == originFlat_ の場合はコアブロック、それ以外は既に伸びた先のセル
	const MapChipIndex fromIndex = field.unflatten(fromFlat_);
	const Vector3 fromPos = field.to_world(fromIndex.x, fromIndex.y, fromIndex.z) - field.center();
	anim.capVisual->transform_mut().set_translate(fromPos + anim.direction * 0.5f);

	if (!anim.capVisual->get_materials().empty() && !baseVisual_->get_materials().empty()) {
		anim.capVisual->get_materials()[0].color = baseVisual_->get_materials()[0].color;
		anim.texture = baseVisual_->get_materials()[0].texture;
	}
	ApplyTexture(anim);

	active = std::move(anim);
}

void ClayStretchAnimator::ApplyTexture(StretchAnimation& anim) {
	if (anim.texture && anim.capVisual && !anim.capVisual->get_materials().empty()) {
		anim.capVisual->get_materials()[0].texture = anim.texture;
	}
}

void ClayStretchAnimator::update(r32 deltaSeconds, MapChipField& field) {
	if (!active) {
		return;
	}

	if (!paramsLoaded) {
		cachedParams = ClayStretchMeshGenerator::LoadParams();
		paramsLoaded = true;
	}

	active->elapsed += std::max(deltaSeconds, 0.0f);
	const r32 normalizedTime = std::clamp(active->elapsed / active->duration, 0.0f, 1.0f);
	const r32 progress = compute_stretch_progress(normalizedTime, cachedParams);

	const i32 frame = select_keyframe(progress);

	if (frame != active->currentFrame && active->capVisual) {
		active->capVisual->reset_mesh(active->keyframeMeshNames[frame]);
		active->currentFrame = frame;
		ApplyTexture(*active);
	}

	if (normalizedTime >= 1.0f) {
		complete(field);
	}
}

void ClayStretchAnimator::finish(MapChipField& field) {
	if (!active) {
		return;
	}
	const i32 last = static_cast<i32>(active->keyframeMeshNames.size()) - 1;
	if (active->capVisual && last >= 0 && active->currentFrame != last) {
		active->capVisual->reset_mesh(active->keyframeMeshNames[last]);
		active->currentFrame = last;
		ApplyTexture(*active);
	}
	complete(field);
}

void ClayStretchAnimator::cancel(MapChipField& field) {
	if (!active) {
		return;
	}

	if (active->capVisual) {
		active->capVisual->reparent(nullptr, true);
		active->capVisual->destroy_self();
		active->capVisual.reset();
	}

	active.reset();
}

void ClayStretchAnimator::complete(MapChipField& field) {
	if (!active) {
		return;
	}
	// adopt_visual から戻ってきても再入しないよう先に active を空にする
	StretchAnimation anim = std::move(*active);
	active.reset();
	field.adopt_visual(anim.toFlat, anim.capVisual);
}
