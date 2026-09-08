#pragma once

#include <span>
#include <string>
#include <unordered_map>

#include <Engine/Assets/Audio/AudioPlayer.h>
#include <Library/Utility/Template/Reference.h>
#include <Library/Utility/Tools/ConstructorMacro.h>

/// <summary>
/// <para>シーン単位の BGM / SE 再生。拡張子付きファイル名で指定する</para>
/// <para>音量とループは cpp の表で決まる。1 音源 1 ボイスなので同じ音は重ならず頭から鳴り直す</para>
/// </summary>
class SoundPlayer {
public:
	SoundPlayer() = default;
	~SoundPlayer() = default;

	SZG_CLASS_MOVE_ONLY(SoundPlayer)

public:
	/// <summary>
	/// Scene::custom_load_asset で呼ぶ。[[game]]/audio の wav を非同期ロード登録する
	/// </summary>
	static void RegisterLoadQue(std::span<const string_literal> names);

	/// <summary>
	/// シーン遷移をまたいで鳴らす SE(決定音・戻る音)。SceneChange の直前に呼ぶ
	/// </summary>
	static void PlayAcrossScene(const std::string& name);

	/// <summary>
	/// custom_setup 以降で呼ぶ。未ロードの名前は警告して飛ばす
	/// </summary>
	void initialize(std::span<const string_literal> names);

	/// 頭から鳴らす(SE 用)
	void restart(const std::string& name);
	/// 再生開始。再生中なら何もしない(ループ BGM・移動音用)
	void play(const std::string& name);
	/// 停止して頭出し
	void stop(const std::string& name);

private:
	Reference<szg::AudioPlayer> find(const std::string& name);

private:
	std::unordered_map<std::string, szg::AudioPlayer> players_;
};
