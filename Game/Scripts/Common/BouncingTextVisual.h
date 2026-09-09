#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <Engine/Module/World/Mesh/Primitive/StringRectInstance.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>

/// @brief 文字列を1文字ずつ独立したStringRectInstanceとして並べ、
///        モンスターハンター風のロード演出のように文字ごとタイミングをずらして上下にバウンドさせる演出
class BouncingTextVisual {
public:
	/// @param basePosition バウンドの接地点（文字列全体の中心・最下点）となる座標
	/// @param fontSize 文字サイズ
	/// @param layer 描画するレイヤー
	void Create(Reference<szg::WorldCluster> world, const std::string& text, const Vector3& basePosition, r32 fontSize = kDefaultFontSize, u32 layer = 1);

	/// @brief バウンドのアニメーションを進める
	void Update(r32 deltaSeconds);

	void SetVisible(bool isVisible);

private:
	std::vector<Reference<szg::StringRectInstance>> characters_;
	std::vector<r32> characterOffsetsX_; // 各文字のbasePosition_からのX方向オフセット（一列に並んだ固定形状）

	Vector3 basePosition_{};
	r32 elapsed_ = 0.0f;
	r32 bounceHeight_ = 0.0f; // 跳ねる高さ。fontSizeに比例させ、Worldの座標スケールに関わらず見た目の比率を保つ

	static constexpr r32 kDefaultFontSize = 48.0f;
	static constexpr r32 kCharSpacingRatio = 0.6f; // fontSizeに対する1文字あたりの送り幅
	static constexpr r32 kBounceHeightRatio = 0.3f; // fontSizeに対する跳ねる高さの比率
	static constexpr r32 kBounceIntervalSeconds = 0.6f; // 1回跳ねる（接地→頂点→接地）のにかかる時間[秒]
	static constexpr r32 kCharDelaySeconds = 0.05f; // 後方の文字ほどこの秒数ずつ遅れて跳ねる（波打つように見せる）
	static constexpr r32 kDepth = 1.0f; // 他UI要素より手前に描画するためのZオフセット
};
