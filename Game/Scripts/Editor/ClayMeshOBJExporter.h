#pragma once

#include <Library/Utility/BuiltinTypes.h>

/// <summary>
/// Blender をバックグラウンドで呼び出して粘土メッシュ OBJ を生成する
/// <para>設定・ステージディレクトリ検証・プロセス起動・出力キャプチャを担当する。</para>
/// </summary>
class ClayMeshOBJExporter {
public:
	/// <summary>
	/// 指定されたステージ番号の粘土メッシュ OBJ を Blender で生成する
	/// <para>成功時は true、失敗時は false。Blender が見つからない・スクリプトが無い・
	/// プロセスがエラー終了した場合などは警告ログを出して false を返す。</para>
	/// </summary>
	static bool Export(i32 stageNumber);

	/// <summary>
	/// 全ステージ（Stage01 ～ Stage{N}）の粘土メッシュ OBJ を Blender でまとめて生成する
	/// <para>各ステージの Export() を順番に呼び出す。途中で失敗しても残りのステージを処理する。</para>
	/// <para>生成に成功したステージ数を返す（0 の場合は Blender パス未検出など）。</para>
	/// </summary>
	static i32 ExportAll();
};
