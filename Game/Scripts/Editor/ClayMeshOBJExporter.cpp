#include "ClayMeshOBJExporter.h"

#include <filesystem>
#include <format>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include <Engine/Application/Logger.h>
#include <Engine/Assets/IAssetBuilder.h>
#include <Library/Utility/Tools/ConvertString.h>

#include "BlenderPathResolver.h"
#include "Scripts/MapChip/MapChipField.h"

namespace {

/// <summary>
/// Blender プロセスを起動して stdout/stderr を文字列として返す
/// </summary>
struct BlenderRunResult {
	bool launched{ false };
	std::string output;
	DWORD exitCode{ 0 };
};

BlenderRunResult run_blender_process(const std::wstring& wCommand) {
	BlenderRunResult result;

	SECURITY_ATTRIBUTES sa = {};
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = nullptr;

	HANDLE hStdOutRead = nullptr;
	HANDLE hStdOutWrite = nullptr;
	if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
		szgWarning("ClayMeshOBJExporter: CreatePipe failed: {}", GetLastError());
		return result;
	}
	SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW si = {};
	si.cb = sizeof(STARTUPINFOW);
	si.hStdError = hStdOutWrite;
	si.hStdOutput = hStdOutWrite;
	si.hStdInput = nullptr;
	si.dwFlags |= STARTF_USESTDHANDLES;

	PROCESS_INFORMATION pi = {};

	// CreateProcessW は第1引数 nullptr、第2引数にコマンドライン全体を受け取る
	BOOL success = CreateProcessW(
		nullptr,
		const_cast<LPWSTR>(wCommand.data()),
		nullptr,
		nullptr,
		TRUE,
		CREATE_NO_WINDOW,
		nullptr,
		nullptr,
		&si,
		&pi
	);

	if (!success) {
		const DWORD err = GetLastError();
		szgWarning("ClayMeshOBJExporter: CreateProcessW failed: {}", err);
		CloseHandle(hStdOutRead);
		CloseHandle(hStdOutWrite);
		return result;
	}

	result.launched = true;

	// 親側では使わないので書き込み側を閉じる
	CloseHandle(hStdOutWrite);

	// 出力を読み取り
	char buffer[4096];
	DWORD bytesRead = 0;
	while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
		buffer[bytesRead] = '\0';
		result.output += buffer;
	}

	// プロセス終了を待機
	WaitForSingleObject(pi.hProcess, INFINITE);

	GetExitCodeProcess(pi.hProcess, &result.exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	CloseHandle(hStdOutRead);

	return result;
}

} // namespace

bool ClayMeshOBJExporter::Export(i32 stageNumber) {
	namespace fs = std::filesystem;

	// Blender の実行ファイルパスを取得（キャッシュ優先）
	const fs::path blenderExe = BlenderPathResolver::Get(true);
	if (blenderExe.empty()) {
		szgWarning(
			"ClayMeshOBJExporter: Blender not found. To use Blender mesh generation, please:\n"
			"  1. Install Blender from https://www.blender.org/download/\n"
			"  2. Set BLENDER_PATH environment variable to blender.exe\n"
			"  3. Or create Game/Assets/clay_mesh_config.json with {{\"blender_path\": \"path/to/blender.exe\"}}\n"
			"  Or click the 'Blender パスを再検出' button in the editor.\n"
			"  Falling back to C++ mesh generation (which may have rendering issues)."
		);
		return false;
	}

	szgInformation("ClayMeshOBJExporter: Using Blender at: {}", blenderExe.string());

	// Python スクリプトのパス ([[game]] を解決)
	const fs::path scriptPath = szg::IAssetBuilder::ResolveFilePath("[[game]]/Models/clay/generate_clay_mesh.py");
	if (!fs::exists(scriptPath)) {
		szgWarning("ClayMeshOBJExporter: generate_clay_mesh.py not found at {}", scriptPath.string());
		return false;
	}

	// 入力ディレクトリ（ステージデータ, [[game]] を解決）
	const fs::path stageDir = szg::IAssetBuilder::ResolveFilePath(MapChipField::StageDirectory(stageNumber), "csv");
	if (!fs::exists(stageDir)) {
		szgWarning("ClayMeshOBJExporter: Stage directory does not exist: {}", stageDir.string());
		return false;
	}

	// 出力ディレクトリ（OBJ ファイル, [[game]] を解決）
	// 末尾にスラッシュを付けない（Windows のコマンドライン解析で \" がエスケープ扱いされる問題を防ぐため）
	const fs::path outputDir = szg::IAssetBuilder::ResolveFilePath("[[game]]/Models/clay/blocks");
	std::error_code ec;
	fs::create_directories(outputDir, ec);

	// Blender をバックグラウンドで実行
	const std::string command = std::format(
		"\"{}\" --background --python \"{}\" -- --stage-dir \"{}\"  --stage-id \"{:02}\" --output-dir \"{}\"",
		blenderExe.string(),
		scriptPath.string(),
		stageDir.string(),
		stageNumber,
		outputDir.string()
	);

	szgInformation("ClayMeshOBJExporter: Generating clay mesh with Blender...");
	szgInformation("ClayMeshOBJExporter: Command: {}", command);

	const std::wstring wCommand = ConvertString(command);
	const BlenderRunResult result = run_blender_process(wCommand);
	if (!result.launched) {
		return false;
	}

	// Blender の出力をログに記録
	if (!result.output.empty()) {
		szgInformation("ClayMeshOBJExporter: Blender output:\n{}", result.output);
	}

	if (result.exitCode != 0) {
		szgWarning("ClayMeshOBJExporter: Blender mesh generation failed with code {}", result.exitCode);
		return false;
	}

	szgInformation("ClayMeshOBJExporter: Clay mesh generation completed successfully");
	return true;
}

i32 ClayMeshOBJExporter::ExportAll() {
	const i32 stageCount = MapChipField::CountStages();
	if (stageCount <= 0) {
		szgWarning("ClayMeshOBJExporter::ExportAll: no stages found");
		return 0;
	}

	// Blender パスを 1 回だけ検証
	const std::filesystem::path blenderExe = BlenderPathResolver::Get(true);
	if (blenderExe.empty()) {
		szgWarning("ClayMeshOBJExporter::ExportAll: Blender not found, aborting all stages");
		return 0;
	}

	szgInformation("ClayMeshOBJExporter::ExportAll: exporting {} stages", stageCount);

	i32 successCount = 0;
	for (i32 stage = 1; stage <= stageCount; ++stage) {
		const bool ok = Export(stage);
		if (ok) {
			++successCount;
		}
		szgInformation(
			"ClayMeshOBJExporter::ExportAll: stage {}/{} (Stage{:02}) {}",
			stage, stageCount, stage, ok ? "succeeded" : "failed");
	}

	szgInformation(
		"ClayMeshOBJExporter::ExportAll: {}/{} stages succeeded",
		successCount, stageCount);
	return successCount;
}
