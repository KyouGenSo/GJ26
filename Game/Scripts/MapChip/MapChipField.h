#pragma once

#include <optional>
#include <string>
#include <vector>

#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Runtime/Scene/World/WorldRoot.h>
#include <Library/Math/Vector3.h>
#include <Library/Utility/Template/Reference.h>

/// <summary>
/// マップチップの種類(CSVのセル値)
/// </summary>
enum class MapChipType : i32 {
	Empty = 0,     // 空
	Clay = 1,      // 粘土
	GoalPiece = 2, // ゴール条件オブジェクト
};

/// <summary>
/// マップチップのグリッド座標
/// </summary>
struct MapChipIndex {
	i32 x{ 0 };
	i32 y{ 0 };
	i32 z{ 0 };

	bool operator==(const MapChipIndex&) const noexcept = default;
};

/// <summary>
/// <para>3Dマップチップ</para>
/// <para>CSV : layer01.csv, layer02.csv, ... の N 番目が y=N-1、行=z(1行目が z=0)、列=x(左→右が +X)</para>
/// <para>チップ(x,y,z)はワールド座標(x,y,z)を中心とする 1x1x1 の立方体</para>
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
	/// Empty 以外のチップに表示用の立方体を生成する
	/// </summary>
	void build(szg::WorldRoot& worldRoot_);

	/// <summary>
	/// チップの取得(範囲外は Empty = 0)
	/// </summary>
	MapChipType get(i32 x, i32 y, i32 z) const;

	/// <summary>
	/// チップの設定(build 済みなら表示も更新、範囲外は無視)
	/// </summary>
	void set(i32 x, i32 y, i32 z, MapChipType type);

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
	void refresh_visual(i32 x, i32 y, i32 z);

private:
	i32 sizeX{ 0 };
	i32 sizeY{ 0 };
	i32 sizeZ{ 0 };
	std::vector<MapChipType> chips;
	std::vector<Reference<szg::StaticMeshInstance>> visuals; // chips と同じ添字、Empty は null
	Reference<szg::WorldRoot> worldRoot; // build 後のみ有効
};
