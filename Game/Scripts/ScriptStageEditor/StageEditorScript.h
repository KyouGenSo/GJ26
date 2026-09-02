#pragma once

#include <vector>

#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Runtime/Input/InputHandler.h>
#include <Engine/Runtime/Scene/World/WorldRoot.h>
#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Library/Utility/Template/Reference.h>

#include "Scripts/MapChip/MapChipField.h"

/// <summary>
/// ステージエディターシーンの3Dプレビュースクリプト
/// </summary>
class StageEditorScript final : public szg::ISceneScript {
public:
	StageEditorScript() = default;
	~StageEditorScript() = default;

	SZG_CLASS_MOVE_ONLY(StageEditorScript)

public:
	void setup(Reference<szg::WorldRoot> worldRoot_);
	void prev_update() override;

private:
	void rebuild();
	static ColorRGB ChipColor(MapChipType type);

private:
	Reference<szg::WorldRoot> worldRoot;
	Reference<szg::StaticMeshInstance> ground;
	std::vector<Reference<szg::StaticMeshInstance>> cubes;
	szg::InputHandler<szg::KeyID> keys;
	u32 lastVersion{ 0 };
};
