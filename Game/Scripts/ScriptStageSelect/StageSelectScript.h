#pragma once

#include <array>

#include <Engine/Runtime/Input/InputHandler.h>
#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Library/Utility/Template/Reference.h>

#include "Scripts/MapChip/MapChipField.h"
#include "Scripts/Manager/SoundPlayer.h"

namespace szg {
class CameraInstance;
class Rect3d;
class StringRectInstance;
class WorldRoot;
} // namespace szg

/// <summary>
/// 選択中のステージと、その前後のステージのプレビュー・切り替え演出を管理する
/// </summary>
class StageSelectScript final : public szg::ISceneScript {
public:
	StageSelectScript() = default;
	~StageSelectScript() override = default;

	SZG_CLASS_MOVE_ONLY(StageSelectScript)

public:
	/// <summary>
	/// SelectScene::custom_load_asset で呼ぶ。このシーンの BGM / SE を登録する
	/// </summary>
	static void RegisterAudioAssets();

	void setup(
		Reference<szg::WorldRoot> worldRoot_,
		Reference<szg::CameraInstance> previewCamera_,
		Reference<szg::StringRectInstance> stageNumberText_,
		Reference<szg::Rect3d> leftArrow_,
		Reference<szg::Rect3d> rightArrow_);
	void prev_update() override;

private:
	struct Preview {
		MapChipField field;
		i32 stageNumber{ 0 };
		i64 carouselIndex{ 0 };
		r32 yawDegrees{ 0.0f };
		r32 startX{ 0.0f };
		r32 targetX{ 0.0f };
		r32 startScale{ 0.0f };
		r32 targetScale{ 0.0f };
		r32 startYaw{ 0.0f };
		r32 targetYaw{ 0.0f };
		bool isUsed{ false };
	};

	/// <summary>
	/// jsonAssetのセットアップ
	/// </summary>
	void setup_json_asset();

	/// <summary>
	/// 選択中のステージと、その前後のステージを初期配置する
	/// </summary>
	void initialize_previews();

	bool build_preview(Preview& preview, i64 carouselIndex, i32 relativeSlot);
	bool begin_transition(i32 step);
	void update_transition(r32 deltaSeconds);
	void finish_transition();

	/// <summary>
	/// 選択中のステージのプレビューを回転させる
	/// </summary>
	void update_selected_rotation(r32 deltaSeconds);

	/// <summary>
	/// 左右の矢印を基準位置の周囲で往復させる
	/// </summary>
	void update_arrow_animation(r32 deltaSeconds);

	/// <summary>
	/// ミニチュアモデルを基準位置の上下でゆっくり往復させる
	/// </summary>
	void update_preview_float_animation(r32 deltaSeconds);

	void update_selection_display();

	/// <summary>
	/// 選択中のステージの前後のステージへ移動する矢印の表示/非表示を切り替える
	/// </summary>
	/// <param name="hasPrevious"></param>
	/// <param name="hasNext"></param>
	void set_navigation_active(bool hasPrevious, bool hasNext);
	Preview* find_preview(i64 carouselIndex);
	Preview* find_unused_preview();
	r32 preview_scale(const Preview& preview, i32 relativeSlot) const;
	r32 slot_position_x(i32 relativeSlot) const;
	r32 slot_yaw(i32 relativeSlot) const;
	r32 arrival_yaw() const;
	i32 wrapped_stage_number(i64 carouselIndex) const;

	/// <summary>
	/// スティックの方向を取得する
	/// </summary>
	/// <returns></returns>
	i32 stick_direction() const;

private:
	// 通常は3個を表示し、4個目は切り替え中に画面外から入るステージへ使用する。
	std::array<Preview, 4> previews;
	// 選択中のステージのプレビューの root(WorldInstance)
	Reference<szg::WorldRoot> worldRoot;
	// ミニチュアの-Z方向を中央到着時に向ける3Dカメラ
	Reference<szg::CameraInstance> previewCamera;
	// 選択中のステージ番号を表示する文字列
	Reference<szg::StringRectInstance> stageNumberText;
	// 選択中のステージの前後のステージへ移動する矢印
	Reference<szg::Rect3d> leftArrow;
	// 選択中のステージの前後のステージへ移動する矢印
	Reference<szg::Rect3d> rightArrow;
	Vector3 leftArrowBasePosition{ CVector3::ZERO };
	Vector3 rightArrowBasePosition{ CVector3::ZERO };
	Vector3 leftArrowBaseScale{ CVector3::ONE };
	Vector3 rightArrowBaseScale{ CVector3::ONE };
	// 選択中のステージ番号の保存と復元に使う
	szg::InputHandler<szg::KeyID> keys;
	// ステージ決定入力
	szg::InputHandler<szg::PadID> pad;
	szg::InputHandler<szg::MouseID> mouse;
	// このシーンの BGM / SE
	SoundPlayer sound;
	// 選択中のステージ番号
	i32 selectedStage{ 1 };
	// 循環境界をまたぐ移動補間に使う連続したインデックス
	i64 selectedCarouselIndex{ 1 };
	// ステージの総数(1 始まり)
	i32 stageCount{ 0 };
	// 前回のスティック方向(-1, 0, 1)
	i32 previousStickDirection{ 0 };
	r32 transitionElapsed{ 0.0f };
	r32 arrowAnimationTime{ 0.0f };
	r32 arrowReactionElapsed{ 0.0f };
	r32 previewFloatAnimationTime{ 0.0f };
	i32 arrowReactionDirection{ 0 };
	bool isTransitioning{ false };
	bool sceneTransitionRequested{ false };

	// プレビュー同士の間隔
	r32 slotSpacing = 4.0f;
	// 選択中のステージのプレビューの高さ
	r32 previewY = -0.25f;
	// 選択中のステージのプレビューのサイズ
	r32 centerPreviewExtent = 2.4f;
	// 選択中のステージの前後のステージのプレビューのサイズ
	r32 sidePreviewExtent = 1.7f;
	// 選択中のステージのプレビューの床の厚さ
	r32 floorThickness = 0.1f;
	// 選択中のステージのプレビューの回転角度(度)
	r32 previewPitchDegrees = 22.0f;
	// 選択中のステージのプレビューの回転速度(度/秒)
	r32 selectedRotationSpeedDegrees = 12.0f;
	// サイドから中央へ移動する間の回転量(度)
	r32 transitionRotationDegrees = 270.0f;
	// スティックの入力を判定する閾値
	r32 stickThreshold = 0.5f;
	// ステージ切り替え演出の時間(秒)
	r32 transitionDuration = 0.4f;

	// 矢印が1往復する時間(秒)
	r32 arrowAnimationPeriod = 1.0f;
	// 矢印が基準位置から動く最大距離
	r32 arrowMoveAmplitude = 0.12f;
	// 入力した方向の矢印が反応する時間(秒)
	r32 arrowReactionDuration = 0.18f;
	// 入力した方向へ矢印が飛び出す距離
	r32 arrowReactionDistance = 0.25f;
	// 入力時の矢印の最大拡大量(0.15なら1.15倍)
	r32 arrowReactionScale = 0.15f;
	// 飛び出した矢印が基準位置側へ跳ね返る距離
	r32 arrowReactionOvershoot = 0.04f;
	// ミニチュアモデルが上下に1往復する時間(秒)
	r32 previewFloatAnimationPeriod = 1.8f;
	// ミニチュアモデルが基準位置から上下に動く最大距離
	r32 previewFloatAmplitude = 0.08f;
};
