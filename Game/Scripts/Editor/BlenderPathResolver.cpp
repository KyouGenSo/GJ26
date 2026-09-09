#include "BlenderPathResolver.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include <json.hpp>

#include <Engine/Application/Logger.h>
#include <Engine/Assets/IAssetBuilder.h>

namespace {

/// <summary>
/// 環境変数 BLENDER_PATH をチェック
/// </summary>
std::optional<std::filesystem::path> check_environment_variable() {
	namespace fs = std::filesystem;
	char envBuffer[1024] = {};
	size_t envSize = 0;
	if (getenv_s(&envSize, envBuffer, sizeof(envBuffer), "BLENDER_PATH") != 0 || envSize == 0) {
		return std::nullopt;
	}
	fs::path p(envBuffer);
	if (fs::exists(p)) {
		return p;
	}
	szgWarning("BlenderPathResolver: BLENDER_PATH '{}' does not exist", envBuffer);
	return std::nullopt;
}

/// <summary>
/// 設定ファイル (Game/Assets/clay_mesh_config.json) をチェック
/// </summary>
std::optional<std::filesystem::path> check_config_file() {
	namespace fs = std::filesystem;
	const fs::path configPath = fs::path("Game/Assets/clay_mesh_config.json");
	if (!fs::exists(configPath)) {
		return std::nullopt;
	}
	try {
		std::ifstream configFile(configPath);
		nlohmann::json config;
		configFile >> config;
		if (config.contains("blender_path")) {
			fs::path p = config["blender_path"].get<std::string>();
			if (fs::exists(p)) {
				return p;
			}
			szgWarning("BlenderPathResolver: clay_mesh_config.json blender_path '{}' does not exist", p.string());
		}
	} catch (const std::exception& e) {
		szgWarning("BlenderPathResolver: Failed to read clay_mesh_config.json: {}", e.what());
	}
	return std::nullopt;
}

/// <summary>
/// PATH 環境変数から blender コマンドを探す (where blender / command -v blender)
/// </summary>
std::optional<std::filesystem::path> check_path_environment() {
	namespace fs = std::filesystem;
	const std::string findCmd =
#ifdef _WIN32
		"where blender.exe >nul 2>&1 && echo FOUND || echo MISSING";
#else
		"command -v blender >/dev/null 2>&1 && echo FOUND || echo MISSING";
#endif
	FILE* pipe = _popen(findCmd.c_str(), "r");
	if (!pipe) {
		return std::nullopt;
	}
	char buffer[256] = {};
	if (fgets(buffer, sizeof(buffer), pipe) == nullptr) {
		_pclose(pipe);
		return std::nullopt;
	}
	std::string result = buffer;
	_pclose(pipe);

	if (result.find("FOUND") == std::string::npos) {
		return std::nullopt;
	}

#ifdef _WIN32
	const std::string getPathCmd = "where blender.exe";
#else
	const std::string getPathCmd = "command -v blender";
#endif
	FILE* pathPipe = _popen(getPathCmd.c_str(), "r");
	if (!pathPipe) {
		return std::nullopt;
	}
	char pathBuffer[1024] = {};
	if (fgets(pathBuffer, sizeof(pathBuffer), pathPipe) == nullptr) {
		_pclose(pathPipe);
		return std::nullopt;
	}
	std::string path = pathBuffer;
	while (!path.empty() && (path.back() == '\n' || path.back() == '\r' || path.back() == ' ')) {
		path.pop_back();
	}
	_pclose(pathPipe);

	if (!path.empty() && fs::exists(path)) {
		return fs::path(path);
	}
	return std::nullopt;
}

/// <summary>
/// 一般的なインストール場所を検索
/// </summary>
std::optional<std::filesystem::path> check_common_install_paths() {
	namespace fs = std::filesystem;
	const std::vector<fs::path> blenderPaths = {
		// Windows - 一般的なインストール場所
		"C:\\Program Files\\Blender Foundation\\Blender\\blender.exe",
		"C:\\Program Files (x86)\\Blender Foundation\\Blender\\blender.exe",
		"D:\\Program Files\\Blender Foundation\\Blender\\blender.exe",
		// バージョン別フォルダ
		"C:\\Program Files\\Blender Foundation\\Blender 5.1\\blender.exe",
		"C:\\Program Files\\Blender Foundation\\Blender 5.0\\blender.exe",
		"C:\\Program Files\\Blender Foundation\\Blender 4.5\\blender.exe",
		"C:\\Program Files\\Blender Foundation\\Blender 4.4\\blender.exe",
		"C:\\Program Files\\Blender Foundation\\Blender 4.3\\blender.exe",
		"C:\\Program Files\\Blender Foundation\\Blender 4.2\\blender.exe",
		"C:\\Program Files\\Blender Foundation\\Blender 4.1\\blender.exe",
		"C:\\Program Files\\Blender Foundation\\Blender 4.0\\blender.exe",
		"C:\\Program Files\\Blender Foundation\\Blender 3.6\\blender.exe",
		"C:\\Program Files\\Blender Foundation\\Blender 3.5\\blender.exe",
		// Windows Store版
		"C:\\WindowsApps\\BlenderFoundation.Blender_*\\blender.exe",
	};

	for (const auto& path : blenderPaths) {
		if (fs::exists(path)) {
			return path;
		}
	}
	return std::nullopt;
}

/// <summary>
/// 環境変数 → 設定ファイル → PATH → 一般場所の順で Blender を検出
/// </summary>
std::optional<std::filesystem::path> detect_blender_path() {
	struct Detector {
		std::optional<std::filesystem::path>(*fn)();
		const char* label;
	};
	const std::array<Detector, 4> detectors{ {
		{ check_environment_variable, "BLENDER_PATH env" },
		{ check_config_file,         "clay_mesh_config.json" },
		{ check_path_environment,    "PATH" },
		{ check_common_install_paths,"common install path" },
	} };

	for (const auto& d : detectors) {
		auto path = d.fn();
		if (path.has_value()) {
			szgInformation("BlenderPathResolver: Blender found via {}: {}", d.label, path->string());
			return path;
		}
	}
	return std::nullopt;
}

/// <summary>
/// キャッシュからパスを読み込む（ファイルが無効なら空のパス）
/// </summary>
std::filesystem::path read_blender_cache(const std::filesystem::path& cachePath) {
	namespace fs = std::filesystem;
	if (!fs::exists(cachePath)) {
		return {};
	}
	try {
		std::ifstream cacheFile(cachePath);
		nlohmann::json cache;
		cacheFile >> cache;
		if (cache.contains("blender_executable_path")) {
			fs::path p = cache["blender_executable_path"].get<std::string>();
			if (fs::exists(p)) {
				return p;
			}
		}
	} catch (const std::exception& e) {
		szgWarning("BlenderPathResolver: Failed to read cache: {}", e.what());
	}
	return {};
}

/// <summary>
/// キャッシュにパスを保存
/// </summary>
void write_blender_cache(const std::filesystem::path& cachePath, const std::filesystem::path& blenderExe) {
	namespace fs = std::filesystem;
	try {
		std::error_code ec;
		fs::create_directories(cachePath.parent_path(), ec);
		nlohmann::json cache;
		cache["blender_executable_path"] = blenderExe.string();
		cache["last_detected"] = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		std::ofstream out(cachePath);
		out << cache.dump(2);
	} catch (const std::exception& e) {
		szgWarning("BlenderPathResolver: Failed to write cache: {}", e.what());
	}
}

/// <summary>
/// キャッシュを削除
/// </summary>
void clear_blender_cache(const std::filesystem::path& cachePath) {
	namespace fs = std::filesystem;
	std::error_code ec;
	fs::remove(cachePath, ec);
}

} // namespace

std::filesystem::path BlenderPathResolver::CacheFilePath() {
	namespace fs = std::filesystem;
	const fs::path cacheDir = szg::IAssetBuilder::ResolveFilePath("[[game]]/DebugData");
	return cacheDir / "blender_path.json";
}

std::filesystem::path BlenderPathResolver::Get(bool useCache) {
	if (useCache) {
		const std::filesystem::path cached = read_blender_cache(CacheFilePath());
		if (!cached.empty()) {
			szgInformation("BlenderPathResolver: Using cached Blender path: {}", cached.string());
			return cached;
		}
	}

	// キャッシュを使う場合でも、無効なパス（ファイル消失等）のために検出は実行する
	const auto detected = detect_blender_path();
	if (detected.has_value()) {
		write_blender_cache(CacheFilePath(), *detected);
		return *detected;
	}
	return {};
}

std::filesystem::path BlenderPathResolver::Redetect() {
	szgInformation("BlenderPathResolver: Force re-detecting Blender path...");
	const auto cachePath = CacheFilePath();
	clear_blender_cache(cachePath);
	const auto detected = detect_blender_path();
	if (detected.has_value()) {
		write_blender_cache(cachePath, *detected);
		szgInformation("BlenderPathResolver: Blender re-detected: {}", detected->string());
		return *detected;
	}
	szgWarning("BlenderPathResolver: Blender re-detection failed: not found");
	return {};
}

std::filesystem::path BlenderPathResolver::GetCached() {
	return read_blender_cache(CacheFilePath());
}
