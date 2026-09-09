#pragma once

#include <string>

#include "BouncingTextVisual.h"
#include "FadeOverlay.h"

/// @brief シーン遷移時の「フェードアウト→ロード完了待ち」演出と、
///        シーン開始時のフェードイン演出をまとめたクラス
/// @details フェード用の全画面Rect3dと、ロード中に表示するバウンドするテキスト（BouncingTextVisual）を
///          セットで管理する。実際のシーン切り替え（SceneManager2::EndSceneChangeIntervalForce）は呼び出し側の責務とし、
///          このクラスは「今切り替えてよいか」だけを報告する。
class LoadingTransitionController {
public:
	struct Config {
		Vector2 fadeSize{}; // フェード用Rect3dのサイズ（そのWorldのカメラ表示範囲を覆う大きさ）
		u32 fadeLayer = 0; // フェードを描画するレイヤー（背景/UIより手前、テキストより奥にする）
		r32 fadeDuration = 0.5f; // フェードアウトにかける時間[秒]
		r32 fadeDepth = -0.03f; // フェード Rect3d の Z 座標。直交投影 World 等で座標系が異なる場合は上書きする
		ColorRGB fadeColor = ColorRGB(0.0f, 0.0f, 0.0f); // フェードで覆う色（RGBのみ。アルファは1まで遷移する）

		std::string loadingText = "NOW LOADING"; // ロード中に表示するテキスト
		Vector3 textBasePosition{}; // テキストがバウンドする接地点（中心座標）
		r32 textFontSize = 48.0f; // BouncingTextVisual::Create参照。そのWorldの座標スケールに合わせる
		u32 textLayer = 0; // テキストを描画するレイヤー。フェードより手前（暗転後も見えるように）にする
	};

	void Create(Reference<szg::WorldCluster> world, const Config& config);

	/// @brief 遷移演出を開始する（フェードアウト+テキストの表示）。2回目以降の呼び出しは無視する。
	void Begin();

	/// @brief シーン開始時のフェードイン演出を開始する（完全な暗転状態から透明へ）。
	/// @note Begin()（フェードアウト）と併用する場合は Begin() を呼ぶ前に本メソッドを呼ぶこと。
	///       ローディングテキストは表示しない。
	void StartFadeIn(r32 duration);

	void Update(r32 deltaSeconds);

	/// @brief フェードインが進行中であればtrue（StartFadeIn 呼び出し後、到達前）
	bool IsFadingIn() const;

	/// @brief フェードインの進行度[0,1]を取得する。StartFadeIn 呼び出し前は 1.0(完了扱い)を返す。
	/// @note フェードイン中の中間アルファから暗転へ戻したい場合の判定用。1.0=完了、0.0=開始直後
	r32 FadeInProgress() const;

	/// @brief フェードインが完了していればtrue
	bool IsFadeInFinished() const;

	/// @brief 内部のフェードオーバーレイの現在アルファ値[0, 1]を返す。フェード途中の中間値もそのまま返す
	r32 CurrentAlpha() const;

	/// @brief フェードアウトが完了し、かつバックグラウンドロードも完了していればtrue
	/// @note trueになった後、実際にシーンを切り替えるかどうかは呼び出し側が判断する。
	bool IsReadyToProceed() const;

private:
	FadeOverlay fadeOverlay_;
	BouncingTextVisual loadingText_;

	r32 fadeDuration_ = 0.5f;
	bool hasStarted_ = false;  // Begin() 呼び済みフラグ（フェードアウト用）
	bool isFadingIn_ = false;  // StartFadeIn() 呼び済みフラグ
};