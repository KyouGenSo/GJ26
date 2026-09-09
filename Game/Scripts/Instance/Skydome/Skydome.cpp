#include "Skydome.h"

namespace {

// カメラの FarClip(1000) より十分小さく、ステージ全体を包む半径
constexpr r32 SKYDOME_RADIUS = 100.0f;

} // namespace

Skydome::Skydome() :
	szg::StaticMeshInstance("skydome.obj") {
	transform_mut().set_scale(CVector3::BASIS * SKYDOME_RADIUS);
	for (auto& material : get_materials()) {
		material.lightingType = szg::LighingType::None;
	}
}
