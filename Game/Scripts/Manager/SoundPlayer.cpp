#include "SoundPlayer.h"

#include <format>
#include <memory>
#include <string_view>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/Audio/AudioLibrary.h>

namespace {

struct SoundSetting {
	r32 volume{ 1.0f };
	bool loop{ false };
};

/// 音量(XAudio2 の振幅倍率)とループ。「BGM より小さく」の指定がある音は BGM の半分
const std::unordered_map<std::string_view, SoundSetting> kSettings{
	{ "titleBgm.wav", { 0.4f, true } },
	{ "selectBgm.wav", { 0.4f, true } },
	{ "gameBgm.wav", { 0.4f, true } },
	{ "clearBgm.wav", { 0.2f, true } },
	{ "sleepingBreath.wav", { 0.1f, false } },
	{ "move.wav", { 0.5f, true } },
	{ "decision.wav", { 0.7f, false } },
	{ "choice.wav", { 0.7f, false } },
	{ "back.wav", { 0.7f, false } },
	{ "jump.wav", { 0.7f, false } },
	{ "grab.wav", { 0.7f, false } },
	{ "cantGrab.wav", { 0.7f, false } },
	{ "reset.wav", { 0.7f, false } },
	{ "undo.wav", { 0.7f, false } },
	{ "stretch.wav", { 0.7f, false } },
	{ "clayConnect.wav", { 0.7f, false } },
	{ "cantMove.wav", { 0.7f, false } },
	{ "objectMove.wav", { 0.7f, false } },
	{ "objectFall.wav", { 0.7f, false } },
	{ "goalConnect.wav", { 0.7f, false } },
	{ "goal.wav", { 0.7f, false } },
};

SoundSetting FindSetting(const std::string& name) {
	const auto found = kSettings.find(name);
	if (found == kSettings.end()) {
		szgWarning("SoundPlayer: '{}' has no volume setting. Default is used.", name);
		return {};
	}
	return found->second;
}

// シーン破棄をまたいで鳴らす SE の置き場。AudioManager::Finalize より後に DestroyVoice しないよう意図的に解放しない(ボイスは XAudio2 終了時に回収される)
std::unique_ptr<szg::AudioPlayer>* const acrossScene = new std::unique_ptr<szg::AudioPlayer>{};

} // namespace

void SoundPlayer::RegisterLoadQue(std::span<const string_literal> names) {
	for (string_literal name : names) {
		szg::AudioLibrary::RegisterLoadQue(std::format("[[game]]/{}", name));
	}
}

void SoundPlayer::PlayAcrossScene(const std::string& name) {
	// 前回の音はここで破棄される(エンジン稼働中なので安全)
	*acrossScene = std::make_unique<szg::AudioPlayer>();
	(*acrossScene)->initialize(name, FindSetting(name).volume);
	(*acrossScene)->play();
}

void SoundPlayer::initialize(std::span<const string_literal> names) {
	for (string_literal name : names) {
		if (!szg::AudioLibrary::IsRegistered(name)) {
			szgWarning("SoundPlayer: '{}' is not loaded.", name);
			continue;
		}
		// AudioPlayer::initialize を 2 回呼ぶとボイスが漏れるので既存は飛ばす
		const auto [player, inserted] = players_.try_emplace(name);
		if (!inserted) {
			continue;
		}
		const SoundSetting setting = FindSetting(name);
		player->second.initialize(name, setting.volume, setting.loop);
	}
}

void SoundPlayer::restart(const std::string& name) {
	if (Reference<szg::AudioPlayer> player = find(name)) {
		player->restart();
	}
}

void SoundPlayer::play(const std::string& name) {
	if (Reference<szg::AudioPlayer> player = find(name)) {
		player->play();
	}
}

void SoundPlayer::stop(const std::string& name) {
	if (Reference<szg::AudioPlayer> player = find(name)) {
		player->stop();
	}
}

Reference<szg::AudioPlayer> SoundPlayer::find(const std::string& name) {
	const auto found = players_.find(name);
	if (found == players_.end()) {
		szgWarning("SoundPlayer: '{}' is not initialized.", name);
		return nullptr;
	}
	return found->second;
}
