#pragma once

#include <array>
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
/// <para>3Dマップチップ</para>
/// <para>CSV : layer01.csv, layer02.csv, ... の N 番目が y=N-1、行=z(1行目が z=0)、列=x(左→右が +X)</para>
/// <para>チップ(x,y,z)はワールド座標(x,y,z)を中心とする 1x1x1 の立方体</para>
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
	/// チップの設定(build 済みなら表示も更新、範囲外は無視)。粘土を置くと全面開放の新しいブロック扱い。color は粘土のときだけ有効
	/// </summary>
	void set(i32 x, i32 y, i32 z, MapChipType type, u8 color = 0);

	/// <summary>
	/// <para>粘土を from から隣接する空セル to へ伸ばす。コアは前後左右へ分岐でき、子はコアから外向きの直線方向にだけ伸ばせる。塞がれた面(stage.json)からは伸ばせない</para>
	/// <para>to がゴール条件オブジェクトなら伸びずにその粘土ブロックがつながる(1 ブロックにつき 1 つ)。つながった粘土はピースと一緒に動く</para>
	/// </summary>
	/// <returns>from が粘土でない / to が空でもピースでもない / 許可された伸長方向でない / その面が塞がれている / 既につながっている ときは false</returns>
	bool stretch_clay(
		const MapChipIndex& from,
		const MapChipIndex& to,
		r32 visualMoveDuration = 0.0f);

	/// <summary>
	/// セルが属する粘土ブロックの伸ばせない面(ClayFace のビット。粘土でない / 範囲外は None)
	/// </summary>
	u8 blocked_faces(const MapChipIndex& index) const;

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
	/// <para>parent(粘土の元セルの立方体)の塞がれた各面に cross.obj を子として付ける。親の destroy_self で一緒に消える</para>
	/// <para>halfSize は親ローカルでの面の半幅、bottomY は親ローカルでの底面の高さ</para>
	/// </summary>
	static void AttachFaceCrosses(szg::WorldRoot& worldRoot_, Reference<szg::WorldInstance> parent, u8 blockedFaces, r32 halfSize, r32 bottomY);

	/// <summary>
	/// <para>ゴール条件オブジェクトを from から to へ動かせるか(from がピース、to が同じ高さで前後左右に隣接する空セル)</para>
	/// <para>つながった粘土も一緒に動くので、粘土の移動先が塞がっていれば false</para>
	/// </summary>
	bool can_move_goal_piece(const MapChipIndex& from, const MapChipIndex& to) const;

	/// <summary>
	/// <para>ゴール条件オブジェクトを from から隣の空セル to へ 1 マス動かす(プレイヤーが掴んで押す・引く 1 歩分)。つながった粘土も一緒に動く</para>
	/// <para>押す: to = ピースの向こう側のセル / 引く: プレイヤーが 1 歩下がった後に to = 元のプレイヤーのセル</para>
	/// </summary>
	/// <returns>can_move_goal_piece が false のときは動かさず false</returns>
	bool move_goal_piece(
		const MapChipIndex& from,
		const MapChipIndex& to,
		r32 visualMoveDuration = 0.0f);

	/// <summary>
	/// stretch_clay / move_goal_piece で開始した表示モデルの移動補間を更新する
	/// </summary>
	void update_visual_interpolation(r32 deltaSeconds);

	/// <summary>
	/// 指定種類の全セル
	/// </summary>
	std::vector<MapChipIndex> find_all(MapChipType type) const;

	/// <summary>
	/// build 済みセルの表示インスタンス(Empty / 未 build / 範囲外は null)
	/// </summary>
	Reference<szg::StaticMeshInstance> visual_mut(const MapChipIndex& index);

	/// <summary>
	/// Goalセルの表示モデルをActive(goal.obj) / Off(goalOff.obj)へ切り替える
	/// </summary>
	/// <returns>Goalセルであり、表示モデルを切り替えられた場合はtrue</returns>
	bool set_goal_active(const MapChipIndex& index, bool active);

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

private:
	bool is_inside(i32 x, i32 y, i32 z) const;
	i32 flat_index(i32 x, i32 y, i32 z) const;
	MapChipIndex unflatten(i32 flat) const;
	std::optional<i32> shifted(i32 flat, const MapChipIndex& delta) const; // flat を delta だけずらしたセル(範囲外は nullopt)
	std::vector<i32> moving_cells(const MapChipIndex& from, const MapChipIndex& to) const; // ピースと、つながった粘土の全セル(動かせない時は空)
	void begin_visual_interpolation(
		const std::vector<i32>& targetCells,
		const Vector3& moveOffset,
		r32 duration);
	void cancel_visual_interpolation();
	void refresh_visual(i32 flat, bool goalActive = false);
	void destroy_root(); // root と子の表示モデルをまとめて破棄

private:
	i32 sizeX{ 0 };
	i32 sizeY{ 0 };
	i32 sizeZ{ 0 };
	std::vector<MapChipType> chips;
	std::vector<i32> clayOrigin; // chips と同じ添字。粘土なら元セルの flat_index、他は -1
	std::vector<i32> clayPiece; // chips と同じ添字。粘土ならつながったゴール条件オブジェクトの flat_index、無ければ -1
	std::vector<u8> clayBlockedFaces; // chips と同じ添字。粘土の元セルにだけ意味がある ClayFace のビット(腕・他は None)
	std::vector<u8> clayColor; // chips と同じ添字。粘土の色番号(ClayColor の添字)、他は 0
	std::vector<Reference<szg::StaticMeshInstance>> visuals; // chips と同じ添字、Empty は null
	struct VisualInterpolation {
		Reference<szg::StaticMeshInstance> visual;
		Vector3 startPosition{ CVector3::ZERO };
		Vector3 targetPosition{ CVector3::ZERO };
	};
	std::vector<VisualInterpolation> visualInterpolations;
	r32 visualInterpolationElapsed{ 0.0f };
	r32 visualInterpolationDuration{ 0.0f };
	Reference<szg::WorldRoot> worldRoot; // build 後のみ有効
	Reference<szg::WorldInstance> root; // build 後のみ有効。破棄すると子の表示モデルも消える
	u32 revision{ 0 };
};
