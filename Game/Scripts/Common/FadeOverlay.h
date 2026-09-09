#pragma once

#include <Engine/Module/World/Mesh/Primitive/Rect3d.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>

/// @brief 画面全体を単色で覆い、アルファ値をアニメーションさせてフェードイン/アウトを行う演出
/// @details Rect3d(Forwardパス)で実装しているため、Deferredパスの副作用(NonLightingPixelの
///          全画面上書き)を経由しない。呼び出し側は各Sceneの最前面レイヤーに配置すること。
class FadeOverlay {
public:
	/// @param size 画面全体を覆うサイズ（そのWorldのカメラ表示範囲に合わせる）
	/// @param layer 描画するレイヤー。そのSceneで最後に描画されるレイヤーを指定し、最前面に出す
	/// @param color 覆う色（RGBのみ。アルファはFadeTo/SetAlphaImmediateで別途制御する）
	void Create(Reference<szg::WorldCluster> world, const Vector2& size, u32 layer, const ColorRGB& color = ColorRGB(0.0f, 0.0f, 0.0f));

	/// @brief 現在のアルファ値からtargetAlphaへduration秒かけて遷移する
	/// @note 既に同じtargetAlphaへ遷移中/到達済みの場合は何もしない（多重発行防止）
	void FadeTo(r32 targetAlpha, r32 duration);

	/// @brief アルファ値を即座に設定する（遷移をキャンセルする）
	void SetAlphaImmediate(r32 alpha);

	/// @brief オーバーレイ Rect3d のワールド座標を設定する。Create 後の Z 補正等に使用する
	void SetTranslate(const Vector3& translate);

	/// @brief オーバーレイ Rect3d への参照を取得する（位置や回転を更に調整したいケース用）
	Reference<szg::Rect3d> overlay_mut() { return overlay_; }

	void Update(r32 deltaSeconds);

	bool IsFading() const;
	r32 CurrentAlpha() const { return currentAlpha_; }

private:
	Reference<szg::Rect3d> overlay_;

	r32 currentAlpha_ = 0.0f;
	r32 startAlpha_ = 0.0f;
	r32 targetAlpha_ = 0.0f;
	r32 duration_ = 0.0f;
	r32 elapsed_ = 0.0f;

	// 同レイヤー内の通常コンテンツ(Z=0付近)より手前に出しつつ、ロード演出のテキスト
	// (BouncingTextVisual::kDepth = -0.05f)より奥にすることで、テキストがフェードの上に見えるようにする。
	static constexpr r32 kOverlayDepth = -0.03f;

	// シーン遷移時の同期読み込みによるフレーム停止で、1フレームのdeltaSecondsが異常に
	// 大きくなることがある。それによってアルファが一気に飛ぶのを防ぐための上限値。
	static constexpr r32 kMaxDeltaSeconds = 1.0f / 30.0f;
};
