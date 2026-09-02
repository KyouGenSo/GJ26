#include "MapTestScene.h"

#include <Engine/Application/Logger.h>
#include <Engine/Module/World/Mesh/StaticMeshInstance.h>
#include <Engine/Runtime/Scene/World/WorldCluster.h>

MapTestScene::MapTestScene() {
	set_name("MapTest");
}

void MapTestScene::custom_setup() {
	Reference<szg::WorldCluster> world = world_mut(0);
	if (!world) {
		szgError("MapTest: world 0 not found.");
		return;
	}
	szg::WorldRoot& root = world->world_root_mut();

	field.load("[[game]]/Map/Test");
	field.build(root);
	const Vector3 center = MapChipField::to_world(field.width() - 1, 0, field.depth() - 1) * 0.5f;

	// 地面
	Reference<szg::StaticMeshInstance> ground = root.instantiate<szg::StaticMeshInstance>(nullptr, "Cube.obj");
	ground->transform_mut().set_scale(Vector3{ static_cast<r32>(field.width()), 0.1f, static_cast<r32>(field.depth()) });
	ground->transform_mut().set_translate(Vector3{ center.x, -0.55f, center.z });
	ground->get_materials()[0].color = ColorRGB{ 0.3f, 0.3f, 0.3f };
}
