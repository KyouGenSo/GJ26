#pragma once

#include <filesystem>

/// <summary>
/// Blender 実行ファイルパスの検出・キャッシュ管理
/// <para>環境変数・設定ファイル・PATH・一般インストール場所の順で探索し、結果を
/// [[game]]/DebugData/blender_path.json にキャッシュする。</para>
/// </summary>
class BlenderPathResolver {
public:
	/// <summary>
	/// Blender の実行ファイルパスを取得する。
	/// <para>useCache=true: キャッシュがあれば即座に返し、無ければ探索して保存する。</para>
	/// <para>useCache=false: キャッシュを無視して環境変数・設定ファイル・PATH・一般場所から再探索する。</para>
	/// <para>見つからなければ空のパスを返す。</para>
	/// </summary>
	static std::filesystem::path Get(bool useCache = true);

	/// <summary>
	/// キャッシュを破棄して Blender パスを強制再検出する（"再探査" ボタン用）。
	/// <para>結果が見つかればキャッシュを更新して返す。見つからなければ空のパスを返す。</para>
	/// </summary>
	static std::filesystem::path Redetect();

	/// <summary>
	/// キャッシュ済みの Blender パスを取得（探索は行わない）。
	/// <para>UI に現在のパスを表示する目的。キャッシュが無ければ空のパスを返す。</para>
	/// </summary>
	static std::filesystem::path GetCached();

	/// <summary>
	/// キャッシュファイルのパスを取得
	/// </summary>
	static std::filesystem::path CacheFilePath();
};
