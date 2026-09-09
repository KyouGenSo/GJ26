#include "LoadingTransitionController.h"

#include <Engine/Runtime/BackgroundLoader/BackgroundLoader.h>

void LoadingTransitionController::Create(Reference<szg::WorldCluster> world, const Config& config) {
	fadeDuration_ = config.fadeDuration;

	fadeOverlay_.Create(world, config.fadeSize, config.fadeLayer, config.fadeColor);
	// FadeOverlay::Create() は既定の Z 座標(kOverlayDepth=-0.03f)を書き込む。直交投影 World など
	// 別座標系に配置する場合は Config::fadeDepth で上書きする
	fadeOverlay_.SetTranslate(Vector3{ 0.0f, 0.0f, config.fadeDepth });
	loadingText_.Create(world, config.loadingText, config.textBasePosition, config.textFontSize, config.textLayer);
	loadingText_.SetVisible(false);
}

void LoadingTransitionController::Begin() {
	if (hasStarted_) {
		return; // 多重発行防止
	}
	hasStarted_ = true;
	// フェードイン途中で Begin() が呼ばれた場合に備え、フェードイン状態を解除する
	isFadingIn_ = false;

	fadeOverlay_.FadeTo(1.0f, fadeDuration_);
	loadingText_.SetVisible(true);
}

void LoadingTransitionController::StartFadeIn(r32 duration) {
	if (isFadingIn_ || hasStarted_) {
		// フェードイン中、もしくは既にフェードアウトを開始している場合は二重起動を防止して何もしない
		return;
	}
	isFadingIn_ = true;

	// 暗転状態から 0 へ。SetAlphaImmediate を FadeTo の前に挟むことで最初のフレームでも黒画面になる
	fadeOverlay_.SetAlphaImmediate(1.0f);
	fadeOverlay_.FadeTo(0.0f, duration);
}

void LoadingTransitionController::Update(r32 deltaSeconds) {
	fadeOverlay_.Update(deltaSeconds);
	loadingText_.Update(deltaSeconds);

	// フェードイン完了の検知。FadeOverlay 側の遷移が終わり、ターゲットに到達していたら完了扱いとする
	if (isFadingIn_ && !fadeOverlay_.IsFading()) {
		isFadingIn_ = false;
	}
}

bool LoadingTransitionController::IsFadingIn() const {
	return fadeOverlay_.IsFading() && !hasStarted_;
}

r32 LoadingTransitionController::FadeInProgress() const {
	// フェードイン中でないなら完了とみなす
	if (!isFadingIn_) {
		return 1.0f;
	}
	// alpha が 1 → 0 へ遷移中なので、(1 - alpha) を進行度とする
	return std::clamp(1.0f - fadeOverlay_.CurrentAlpha(), 0.0f, 1.0f);
}

bool LoadingTransitionController::IsFadeInFinished() const {
	return !isFadingIn_ && !fadeOverlay_.IsFading();
}

r32 LoadingTransitionController::CurrentAlpha() const {
	return fadeOverlay_.CurrentAlpha();
}

bool LoadingTransitionController::IsReadyToProceed() const {
	return hasStarted_ && !fadeOverlay_.IsFading() && !szg::BackgroundLoader::IsLoading();
}