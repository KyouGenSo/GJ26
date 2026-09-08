#pragma once

#include <Engine/Module/World/Mesh/StaticMeshInstance.h>

/// <summary>
/// 背景の天球。内向きの球メッシュ skydome.obj をライティング無しで描画する
/// </summary>
class Skydome final : public szg::StaticMeshInstance {
public:
	Skydome();
	~Skydome() noexcept override = default;
	SZG_CLASS_MOVE_ONLY(Skydome)
};
