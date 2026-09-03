#pragma once

#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Runtime/Input/InputHandler.h>
#include <Engine/Runtime/Scene/World/WorldRoot.h>
#include <Engine/Runtime/SceneScript/ISceneScript.h>
#include <Library/Utility/Template/Reference.h>

#include "Scripts/MapChip/MapChipField.h"

/// <summary>
/// <para>マップチップの表示確認用スクリプト</para>
/// <para>起動時は Temp/StageNumber のステージ(無ければ 1)を読み、← →(LB / RB)で前後のステージに切り替える</para>
/// </summary>
class MapTestScript final : public szg::ISceneScript {
public:
	MapTestScript() = default;
	~MapTestScript() = default;

	SZG_CLASS_MOVE_ONLY(MapTestScript)

public:
	void setup(Reference<szg::WorldRoot> worldRoot_);
	void prev_update() override;

	MapChipField& field_mut() { return field; }

	/// <summary>
	/// Player の操作対象にするマーカー(青い小さな立方体)
	/// </summary>
	Reference<szg::WorldInstance> marker_mut() { return marker; }

private:
	void reload();

private:
	MapChipField field;
	Reference<szg::WorldRoot> worldRoot;
	Reference<szg::StaticMeshInstance> ground;
	Reference<szg::StaticMeshInstance> marker;
	szg::InputHandler<szg::KeyID> keys;
	szg::InputHandler<szg::PadID> pad;
	i32 stageNumber{ 1 };
	i32 stageCount{ 0 };
};
