#pragma once

#include <array>
#include <deque>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Module/World/WorldInstance/WorldInstance.h>
#include <Engine/Runtime/Scene/World/WorldRoot.h>
#include <Library/Math/ColorRGB.h>
#include <Library/Math/Vector3.h>
#include <Library/Utility/Template/Reference.h>

/// <summary>
/// マップチップの種類(CSVのセル値)
/// </summary>
enum class MapChipType : i32 {
	Empty = 0,     // 空
	Clay = 1,      // 粘土
	GoalPiece = 2, // ゴール条件オブジェクト
	Goal = 3,      // ゴール
	GoalPieceUpper = 4, // ゴール条件オブジェクトの上段(CSV には書かず、load が GoalPiece の真上へ補完する)
};

/// <summary>
/// マップチップのグリッド座標
/// </summary>
struct MapChipIndex {
	i32 x;
	i32 y;
	i32 z;

	bool operator==(const MapChipIndex&) const = default;
};

/// <summary>
/// 粘土ブロックの伸ばせない面(元セルから見た水平 4 方向)のビット
/// </summary>
namespace ClayFace {

inline constexpr u8 None = 0;
inline constexpr u8 PosX = 1;
inline constexpr u8 NegX = 2;
inline constexpr u8 PosZ = 4;
inline constexpr u8 NegZ = 8;

/// <summary>
/// ビット / stage.json の表記 / グリッド方向 の対応
/// </summary>
struct Entry {
	u8 bit;
	const char* name;
	MapChipIndex direction;
};

inline constexpr std::array<Entry, 4> Table{ {
	{ PosX, "+X", { 1, 0, 0 } },
	{ NegX, "-X", { -1, 0, 0 } },
	{ PosZ, "+Z", { 0, 0, 1 } },
	{ NegZ, "-Z", { 0, 0, -1 } },
} };

/// <summary>
/// 方向 (±1,0,0) / (0,0,±1) → ビット(該当なしは None)
/// </summary>
constexpr u8 FromDirection(const MapChipIndex& direction) {
	for (const Entry& entry : Table) {
		if (entry.direction == direction) {
			return entry.bit;
		}
	}
	return None;
}

/// <summary>
/// "+X" 等の表記 → ビット(不明は None)
/// </summary>
constexpr u8 FromName(std::string_view name) {
	for (const Entry& entry : Table) {
		if (name == entry.name) {
			return entry.bit;
		}
	}
	return None;
}

/// <summary>
/// ビット → グリッド方向(該当なしは +Z)
/// </summary>
constexpr MapChipIndex ToDirection(u8 bit) {
	for (const Entry& entry : Table) {
		if (entry.bit == bit) {
			return entry.direction;
		}
	}
	return { 0, 0, 1 };
}

/// <summary>
/// ビット → stage.json の表記(該当なしは "+Z")
/// </summary>
constexpr const char* ToName(u8 bit) {
	for (const Entry& entry : Table) {
		if (entry.bit == bit) {
			return entry.name;
		}
	}
	return "+Z";
}

} // namespace ClayFace

/// <summary>
/// 粘土ブロックの色(テクスチャ差し替え)。0 は clay.obj 既定の clay.png
/// </summary>
namespace ClayColor {

inline constexpr i32 Count = 5;

/// <summary>
/// 色番号 → テクスチャ名(TextureLibrary::GetTexture に渡す)
/// </summary>
inline constexpr std::array<const char*, Count> Textures{
	"clay.png", "clay2.png", "clay3.png", "clay4.png", "clay5.png",
};

/// <summary>
/// 色番号 → エディタ表示用の代表色(テクスチャの平均色)
/// </summary>
inline constexpr std::array<ColorRGB, Count> Preview{
	ColorRGB{ 0.965f, 0.961f, 0.957f },
	ColorRGB{ 1.000f, 0.682f, 0.920f },
	ColorRGB{ 0.693f, 0.702f, 1.000f },
	ColorRGB{ 0.724f, 1.000f, 0.674f },
	ColorRGB{ 1.000f, 0.938f, 0.674f },
};

/// <summary>
/// 伸長方向 → 矢印付きテクスチャの接尾辞(ClayFace::Table と同じ並び)
/// </summary>
inline constexpr std::array<const char*, 4> ArrowSuffix{ "px", "nx", "pz", "nz" };

/// <summary>
/// 伸ばして出た粘土用の矢印付きテクスチャ名(例: clay2_px.png)。face は ClayFace のビット 1 つ
/// </summary>
inline std::string ArrowTexture(i32 color, u8 face) {
	std::string_view stem = Textures[color];
	stem.remove_suffix(4); // ".png"
	for (size_t i = 0; i < ClayFace::Table.size(); ++i) {
		if (ClayFace::Table[i].bit == face) {
			return std::format("{}_{}.png", stem, ArrowSuffix[i]);
		}
	}
	return Textures[color];
}

} // namespace ClayColor

/// <summary>
/// stage.json の "Clay" 1 件(粘土の元セルの位置・伸ばせない面・色)
/// </summary>
struct ClayRecord {
	MapChipIndex position;
	u8 blockedFaces;
	u8 color{ 0 }; // ClayColor の番号
};

/// <summary>
/// stage.json の "PlayerSpawn"(プレイヤーの初期セルと向き)
/// </summary>
struct PlayerSpawnRecord {
	MapChipIndex position;
	u8 direction{ ClayFace::PosZ }; // ClayFace のビット 1 つ
};

/// <summary>
/// (x, z) の列で y を床に合わせる。埋まっていれば上の最初の空セルへ、浮いていれば下が空でなくなるまで下げる(y=0 が地面)。空セルが無ければ nullopt
/// </summary>
/// <param name="get">(x, y, z) → MapChipType。範囲外は Empty を返すこと</param>
template<typename GetChip>
std::optional<i32> SnapToFloorY(i32 x, i32 y, i32 z, i32 height, GetChip get) {
	while (y < height && get(x, y, z) != MapChipType::Empty) {
		++y;
	}
	if (y >= height) {
		return std::nullopt;
	}
	while (y > 0 && get(x, y - 1, z) == MapChipType::Empty) {
		--y;
	}
	return y;
}

/// <summary>
/// <para>3Dマップチップ</para>
/// <para>CSV : layer01.csv, layer02.csv, ... の N 番目が y=N-1、行=z(1行目が z=0)、列=x(左→右が +X)</para>
/// <para>チップ(x,y,z)はワールド座標(x,y,z)を中心とする 1x1x1 の立方体</para>
/// <para>ゴール条件オブジェクトは 2 セル高。真上のセルは GoalPieceUpper として塞がり、ピースと一緒に動く</para>
/// <para>立方体は root_mut()(ステージ中央の空 WorldInstance)の子。全体の縮小・移動は root の transform で行う(to_world / to_index は root が単位のときのグリッド配置)</para>
/// </summary>
class MapChipField {
public:
	/// <summary>
	/// directory/layer01.csv, layer02.csv, ... を連番が途切れるまで読み込む
	/// </summary>
	/// <param name="directory">"[[game]]/Map/Stage01" 形式のディレクトリ</param>
	/// <returns>1層以上読み込めたら true</returns>
	bool load(const std::string& directory);

	/// <summary>
	/// StageDirectory(stageNumber) を load する
	/// </summary>
	bool load_stage(i32 stageNumber);

	/// <summary>
	/// "[[game]]/Map/Stage01" 形式のステージディレクトリ(番号は 1 始まり、0 埋め 2 桁)
	/// </summary>
	static std::string StageDirectory(i32 stageNumber);

	/// <summary>
	/// Stage01, Stage02, ... の layer01.csv が存在する間数える
	/// </summary>
	static i32 CountStages();

	/// <summary>
	/// 表示に使用する Cube / Clay / GoalPiece / Goal のメッシュをロードキューへ登録する
	/// </summary>
	static void RegisterVisualAssets();

	/// <summary>
	/// ステージ中央に root を作り、Empty 以外のチップに種類別の表示モデルをその子として生成する
	/// </summary>
	void build(szg::WorldRoot& worldRoot_);

	/// <summary>
	/// Goal表示に使う描画レイヤーを設定する（build前に指定。既定はLayer 0）
	/// </summary>
	void set_goal_visual_layer(u32 layer) noexcept { goalVisualLayer = layer; }

	/// <summary>
	/// 掴める対象の輪郭(反転ハル)の見た目。thickness は拡大率の増分(0.06 で 6% 大きい)
	/// </summary>
	struct HighlightStyle {
		ColorRGB color{ 1.0f, 0.9f, 0.2f };
		r32 thickness{ 0.075f };
	};

	/// <summary>
	/// 指定セルの表示に輪郭を付ける(同時に 1 つ)。nullopt / 範囲外 / 空セルで消す。同じセルなら何もしない
	/// </summary>
	void set_highlight(const std::optional<MapChipIndex>& index);

	/// <summary>
	/// 輪郭の見た目を変える。表示中の輪郭にも即反映する
	/// </summary>
	void set_highlight_style(const HighlightStyle& style);

	/// <summary>
	/// 全表示モデルの親(ステージ中央に置いた空の WorldInstance)。build 前は null。子のローカル座標は中央基準
	/// </summary>
	Reference<szg::WorldInstance> root_mut() { return root; }

	/// <summary>
	/// ステージ全体の中央(グリッド座標系)。build 時の root の位置
	/// </summary>
	Vector3 center() const;

	/// <summary>
	/// チップの取得(範囲外は Empty = 0)
	/// </summary>
	MapChipType get(i32 x, i32 y, i32 z) const;

	/// <summary>
	/// チップの設定(build 済みなら表示も更新、範囲外は無視)。粘土を置くと全面開放の新しいブロック扱い。color は粘土のときだけ有効。GoalPiece を置いても上段は補完しない
	/// </summary>
	void set(i32 x, i32 y, i32 z, MapChipType type, u8 color = 0);

	/// <summary>
	/// <para>粘土を from から隣接する空セル to へ伸ばす。コアは前後左右へ分岐でき、子はコアから外向きの直線方向にだけ伸ばせる。塞がれた面(stage.json)からは伸ばせない</para>
	/// <para>to がゴール条件オブジェクトなら伸びずに、その粘土ブロックと粘土づたいに面で接している(上下含む)未接続の粘土ブロック全部がつながる(1 ブロックにつき 1 つ)。つながった粘土はピースと一緒に動く</para>
	/// </summary>
	/// <returns>from が粘土でない / to が空でもピースでもない / 許可された伸長方向でない / その面が塞がれている(このとき warn_blocked_face の演出を始める) / 既につながっている ときは false</returns>
	bool stretch_clay(
		const MapChipIndex& from,
		const MapChipIndex& to,
		r32 visualMoveDuration = 0.0f);

	/// <summary>
	/// 指定セルが粘土ブロックのコアか（子Clay・粘土以外・範囲外は false）
	/// </summary>
	bool is_clay_core(const MapChipIndex& index) const;

	/// <summary>
	/// セルが属する粘土ブロックの伸ばせない面(ClayFace のビット。粘土でない / 範囲外は None)
	/// </summary>
	u8 blocked_faces(const MapChipIndex& index) const;

	/// <summary>
	/// index が属する粘土ブロックのコアの direction 側の面が塞がれていれば、その面の cross を一定時間 赤点滅・振動させる(塞がれていなければ何もしない)
	/// </summary>
	void warn_blocked_face(const MapChipIndex& index, const MapChipIndex& direction);

	/// <summary>
	/// index のブロック(粘土なら同じ元セルの全セル、ゴール条件オブジェクトなら下段・上段とつながった粘土)を direction 方向に一定時間振動させる(動かせなかった通知)。表示の移動補間中は何もしない
	/// </summary>
	void warn_block_stuck(const MapChipIndex& index, const MapChipIndex& direction);

	/// <summary>
	/// warn_blocked_face / warn_block_stuck / stretch_clay で始めた演出を更新する
	/// </summary>
	void update_warnings(r32 deltaSeconds);

	/// <summary>
	/// セルの粘土の色番号(ClayColor の添字。粘土でない / 範囲外は 0)
	/// </summary>
	u8 clay_color(const MapChipIndex& index) const;

	/// <summary>
	/// directory/stage.json の "Clay" を読む。ファイルが無ければ空(警告なし)、壊れていれば警告して空。セルが粘土かの検証は呼び出し側
	/// </summary>
	static std::vector<ClayRecord> LoadStageJsonClay(const std::string& directory);

	/// <summary>
	/// directory/stage.json の "Clay" を records で書き換える(他のキーは維持、無ければ作る)
	/// </summary>
	static bool SaveStageJsonClay(const std::string& directory, const std::vector<ClayRecord>& records);

	/// <summary>
	/// directory/stage.json の "PlayerSpawn" を読む。ファイルかキーが無ければ nullopt(警告なし)、壊れていれば警告して nullopt。セルが空かの検証は呼び出し側
	/// </summary>
	static std::optional<PlayerSpawnRecord> LoadStageJsonPlayerSpawn(const std::string& directory);

	/// <summary>
	/// directory/stage.json の "PlayerSpawn" を spawn で書き換える(nullopt ならキーを消す。他のキーは維持、無ければ作る)
	/// </summary>
	static bool SaveStageJsonPlayerSpawn(const std::string& directory, const std::optional<PlayerSpawnRecord>& spawn);

	/// <summary>
	/// stage.json のプレイヤー初期位置(無い / 空セルでない場合は nullopt)
	/// </summary>
	const std::optional<PlayerSpawnRecord>& player_spawn() const { return playerSpawn; }

	/// <summary>
	/// <para>parent(粘土の元セルの立方体)の塞がれた各面に cross.obj を子として付ける。親の destroy_self で一緒に消える</para>
	/// <para>halfSize は親ローカルでの面の半幅、bottomY は親ローカルでの底面の高さ</para>
	/// </summary>
	/// <returns>付けた cross(ClayFace::Table と同じ並び、付けなかった面は null)</returns>
	static std::array<Reference<szg::StaticMeshInstance>, 4> AttachFaceCrosses(szg::WorldRoot& worldRoot_, Reference<szg::WorldInstance> parent, u8 blockedFaces, r32 halfSize, r32 bottomY);

	/// <summary>
	/// <para>つながった粘土・ゴール条件オブジェクトを光らせる複製(visual と同じメッシュ・マテリアル)を visual の子として付ける。親の destroy_self で一緒に消える</para>
	/// <para>描画レイヤー 1 (RenderPath.json でぼかしてブルーム合成される) にライティング無しで描き、深度テストに勝つよう少し大きくする</para>
	/// </summary>
	static void AttachGlow(szg::WorldRoot& worldRoot_, Reference<szg::StaticMeshInstance> visual);

	/// <summary>
	/// <para>掴める対象の輪郭として、面の向きを反転した少し大きいメッシュ(X_outline.obj)を visual の子に付ける。親の destroy_self で一緒に消える</para>
	/// <para>バックフェースカリングで奥側の面だけが描かれ、手前の本体に隠されて縁だけが残る</para>
	/// </summary>
	/// <returns>付けた輪郭。visual のメッシュに対応する輪郭メッシュが無ければ null</returns>
	static Reference<szg::StaticMeshInstance> AttachOutline(szg::WorldRoot& worldRoot_, Reference<szg::StaticMeshInstance> visual, const HighlightStyle& style);

	/// <summary>
	/// <para>ゴール条件オブジェクトを from から to へ動かせるか(from がピース、to が同じ高さで前後左右に隣接する空セル)</para>
	/// <para>つながった粘土も一緒に動くので、粘土の移動先が塞がっていれば false</para>
	/// </summary>
	bool can_move_goal_piece(const MapChipIndex& from, const MapChipIndex& to) const;

	/// <summary>
	/// <para>ゴール条件オブジェクトを from から隣の空セル to へ 1 マス動かす(プレイヤーが掴んで押す・引く 1 歩分)。つながった粘土も一緒に動く</para>
	/// <para>押す: to = ピースの向こう側のセル / 引く: プレイヤーが 1 歩下がった後に to = 元のプレイヤーのセル</para>
	/// <para>動かした後、グループ全セルの下が空なら着地するまで落ちる(表示は横移動の補間が終わってから落ちる)</para>
	/// </summary>
	/// <returns>着地後のピースの位置。can_move_goal_piece が false のときは動かさず nullopt</returns>
	std::optional<MapChipIndex> move_goal_piece(
		const MapChipIndex& from,
		const MapChipIndex& to,
		r32 visualMoveDuration = 0.0f);

	/// <summary>
	/// stretch_clay / move_goal_piece で開始した表示モデルの移動補間を更新する
	/// </summary>
	void update_visual_interpolation(r32 deltaSeconds);

	/// <summary>
	/// 表示モデルの移動補間が再生中か
	/// </summary>
	bool is_visual_interpolating() const { return !visualInterpolationSteps.empty(); }

	/// <summary>
	/// 指定種類の全セル
	/// </summary>
	std::vector<MapChipIndex> find_all(MapChipType type) const;

	/// <summary>
	/// build 済みセルの表示インスタンス(Empty / 未 build / 範囲外は null)
	/// </summary>
	Reference<szg::StaticMeshInstance> visual_mut(const MapChipIndex& index);

	/// <summary>
	/// セルデータの複製(表示・root は含まない)。restore で戻す
	/// </summary>
	struct Cells {
		std::vector<MapChipType> chips;
		std::vector<i32> clayOrigin;
		std::vector<i32> clayPiece;
		std::vector<u8> clayBlockedFaces;
		std::vector<u8> clayColor;
	};

	Cells cells() const { return Cells{ chips, clayOrigin, clayPiece, clayBlockedFaces, clayColor }; }

	/// <summary>
	/// cells() の内容に戻し、変わったセルだけ表示を作り直す。サイズが違えば警告して無視。version は進む
	/// </summary>
	void restore(const Cells& source);

	/// <summary>
	/// load / set / stretch_clay / move_goal_piece / restore のたびに増える。他システムが再判定するためのトリガー
	/// </summary>
	u32 version() const { return revision; }

	/// <summary>
	/// グリッド座標 → ワールド座標(チップ中心)
	/// </summary>
	static Vector3 to_world(i32 x, i32 y, i32 z);

	/// <summary>
	/// ワールド座標 → グリッド座標(範囲外は nullopt)
	/// </summary>
	std::optional<MapChipIndex> to_index(const Vector3& position) const;

	/// <summary>
	/// グリッド座標がフィールド内か
	/// </summary>
	bool contains(const MapChipIndex& index) const;

	i32 width() const { return sizeX; }
	i32 height() const { return sizeY; }
	i32 depth() const { return sizeZ; }

	/// <summary>
	/// root と子の表示モデルをまとめて破棄する(chips は残るので load し直せる)
	/// </summary>
	void destroy_root();

private:
	bool is_inside(i32 x, i32 y, i32 z) const;
	i32 flat_index(i32 x, i32 y, i32 z) const;
	MapChipIndex unflatten(i32 flat) const;
	std::optional<i32> shifted(i32 flat, const MapChipIndex& delta) const; // flat を delta だけずらしたセル(範囲外は nullopt)
	u8 clay_stretch_face(i32 flat) const; // 伸ばして出た粘土がコアから伸びた方向(ClayFace のビット。コア・粘土以外は None)
	bool has_connected_clay(i32 piece) const; // piece(ゴール条件オブジェクトの下段)につながった粘土が 1 つでもあるか
	void set_highlight_flat(std::optional<i32> flat); // set_highlight の本体(flat_index 版)
	std::vector<i32> moving_cells(const MapChipIndex& from, const MapChipIndex& to) const; // ピースと、つながった粘土の全セル(動かせない時は空)
	std::vector<i32> relocate_cells(const std::vector<i32>& cells, const MapChipIndex& delta); // cells を delta だけずらして置き直し、移動後の flat 一覧を返す(表示も更新)
	struct VisualMove {
		Vector3 offset;
		r32 duration;
	};
	void begin_visual_interpolation(
		const std::vector<i32>& targetCells,
		const std::vector<VisualMove>& moves); // moves を順に再生する。表示は現在位置を最終位置として moves の合計分だけ戻した所から始まる
	void cancel_visual_interpolation();
	void begin_blocked_face_warning(i32 root, u8 bit); // root(コア)の bit の面の cross を赤点滅・振動させる(cross が無ければ何もしない)
	struct WarningEffect;
	void begin_warning(i32 flat, Reference<szg::StaticMeshInstance> visual, const Vector3& shakeAxis, r32 shakeAmplitude, bool blink); // 同じ visual の演出があれば戻してから始める
	void end_warning(WarningEffect& warning); // 色と位置を戻す
	void end_warnings(); // 全演出を戻して消す
	void refresh_visual(i32 flat);

private:
	i32 sizeX{ 0 };
	i32 sizeY{ 0 };
	i32 sizeZ{ 0 };
	std::vector<MapChipType> chips;
	std::vector<i32> clayOrigin; // chips と同じ添字。粘土なら元セルの flat_index、他は -1
	std::vector<i32> clayPiece; // chips と同じ添字。粘土ならつながったゴール条件オブジェクトの flat_index、無ければ -1
	std::vector<u8> clayBlockedFaces; // chips と同じ添字。粘土の元セルにだけ意味がある ClayFace のビット(腕・他は None)
	std::vector<u8> clayColor; // chips と同じ添字。粘土の色番号(ClayColor の添字)、他は 0
	u32 goalVisualLayer{ 0 };
	std::optional<i32> highlightFlat; // 輪郭を付けるセル。表示が作り直されても refresh_visual が付け直す
	Reference<szg::StaticMeshInstance> highlight; // highlightFlat の表示の子。親が消えるときは refresh_visual / destroy_root で捨てる
	HighlightStyle highlightStyle;
	std::optional<PlayerSpawnRecord> playerSpawn;
	std::vector<Reference<szg::StaticMeshInstance>> visuals; // chips と同じ添字、Empty は null
	struct VisualInterpolation {
		Reference<szg::StaticMeshInstance> visual;
		Vector3 startPosition{ CVector3::ZERO };
		Vector3 targetPosition{ CVector3::ZERO };
	};
	struct VisualInterpolationStep {
		std::vector<VisualInterpolation> entries;
		r32 duration{ 0.0f };
	};
	std::deque<VisualInterpolationStep> visualInterpolationSteps; // front が再生中
	r32 visualInterpolationElapsed{ 0.0f };
	std::vector<std::array<Reference<szg::StaticMeshInstance>, 4>> faceCrosses; // chips と同じ添字。粘土のコアにだけ入る ClayFace::Table 並びの cross(無い面は null)
	struct WarningEffect {
		i32 flat; // refresh_visual で捨てるための添字(cross はコア、ブロックはそのセル)
		Reference<szg::StaticMeshInstance> visual;
		Vector3 basePosition; // 親ローカルでの元の位置
		Vector3 shakeAxis;
		r32 shakeAmplitude;
		std::vector<ColorRGB> baseColors; // マテリアルごとの元の色。空なら点滅しない
		r32 elapsed{ 0.0f };
		i32 frames{ 0 }; // 更新回数。揺れの向きはフレーム単位で反転させる(時間基準だとフレームレートとの折り返しで幅が揺らぐ)
	};
	std::vector<WarningEffect> warnings; // 再生中の演出
	Reference<szg::WorldRoot> worldRoot; // build 後のみ有効
	Reference<szg::WorldInstance> root; // build 後のみ有効。破棄すると子の表示モデルも消える
	u32 revision{ 0 };
};
