#include "AxisController.h"

#include <Engine/Runtime/RuntimeStorage/RuntimeStorage.h>
#include <Engine/Module/World/Mesh/StaticMeshInstance.h>

void AxisController::setup() {
	axisInstance = 
		szg::RuntimeStorage::GetValue<Reference<szg::StaticMeshInstance>>(
			"RuntimeInstance", "Axis"
		)
		.value_or(nullptr);
}

void AxisController::post_update() {
	if (!axisInstance) {
		return;
	}

	Reference<const szg::WorldInstance> parent = axisInstance->parent_imm();

	if (!parent) {
		return;
	}

	Quaternion parentQuat = parent->world_affine().get_basis().to_quaternion();

	axisInstance->transform_mut().set_quaternion(parentQuat.inverse());
}
