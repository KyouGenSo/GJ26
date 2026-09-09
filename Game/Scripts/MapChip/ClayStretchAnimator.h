#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <Library/Math/Vector3.h>
#include <Library/Utility/BuiltinTypes.h>
#include <Library/Utility/Template/Reference.h>

#include "ClayStretchMeshGenerator.h"

class MapChipField;

namespace szg {
class WorldRoot;
class WorldInstance;
class StaticMeshInstance;
class TextureAsset;
} // namespace szg

class ClayStretchAnimator {
public:
	void begin(
		i32 originFlat,
		i32 fromFlat,
		i32 toFlat,
		const Vector3& direction,
		r32 duration,
		MapChipField& field,
		szg::WorldRoot& worldRoot,
		Reference<szg::WorldInstance> root,
		Reference<szg::StaticMeshInstance> baseVisual);

	void update(r32 deltaSeconds, MapChipField& field);
	void cancel(MapChipField& field);

	/// <summary>
	/// 再生中なら最終形にして cap を field に引き取らせる(セルを書き換える前に呼ぶ)
	/// </summary>
	void finish(MapChipField& field);

	bool is_animating() const { return active.has_value(); }

private:
	struct StretchAnimation {
		i32 originFlat;
		i32 fromFlat;
		i32 toFlat;
		Vector3 direction;
		r32 elapsed;
		r32 duration;
		std::vector<std::string> keyframeMeshNames;
		i32 currentFrame;
		Reference<szg::StaticMeshInstance> capVisual;
		Reference<szg::StaticMeshInstance> baseVisual;
		std::shared_ptr<const szg::TextureAsset> texture; // 伸ばし元の表示のテクスチャ。reset_mesh が既定に戻すのでフレーム切替のたびに当て直す
	};

	std::optional<StretchAnimation> active;
	ClayStretchParams cachedParams;
	bool paramsLoaded = false;

	r32 compute_stretch_progress(r32 normalizedTime, const ClayStretchParams& params) const;
	i32 select_keyframe(r32 progress) const;
	void complete(MapChipField& field); // cap を field.adopt_visual に渡して終了する
	static void ApplyTexture(StretchAnimation& anim);
};
