#include "UndoManager.h"

#include <Engine/Application/Logger.h>
#include <Engine/Runtime/Input/Input.h>

#include "Scripts/Instance/Player/Player.h"

namespace {

constexpr r32 kTriggerThreshold = 0.5f; // LT をボタン扱いする閾値
constexpr size_t kMaxHistory = 1024;

} // namespace

void UndoManager::setup(Reference<MapChipField> field_, Reference<Player> player_) {
	field = field_;
	player = player_;
	keys.initialize({ szg::KeyID::Z }, szg::InputInitializeMode::Current);
	pad.initialize({ szg::PadID::LShoulder }, szg::InputInitializeMode::Current);
	clear();
}

void UndoManager::prev_update() {
	if (!field) {
		return;
	}
	keys.update();
	pad.update();
	const bool triggerL = szg::Input::TriggerL() >= kTriggerThreshold;
	const bool triggerLDown = triggerL && !triggerLPressed;
	triggerLPressed = triggerL;

	if (keys.trigger(szg::KeyID::Z) || pad.trigger(szg::PadID::LShoulder) || triggerLDown) {
		undo();
	}
	pending = capture();
}

void UndoManager::post_update() {
	if (!field || field->version() == lastVersion) {
		return;
	}
	lastVersion = field->version();
	history.push_back(std::move(pending));
	if (history.size() > kMaxHistory) {
		history.pop_front();
	}
}

void UndoManager::clear() {
	history.clear();
	lastVersion = field ? field->version() : 0;
}

void UndoManager::undo() {
	if (history.empty()) {
		return;
	}
	apply(history.back());
	history.pop_back();
	// restore で version が進むので、post_update で積み直さないよう揃える
	lastVersion = field->version();
	szgInformation("UndoManager: undo (remaining {})", history.size());
}

UndoManager::Snapshot UndoManager::capture() const {
	Snapshot snapshot;
	snapshot.cells = field->cells();
	const Reference<const szg::WorldInstance> instance = player ? player->get_world_instance_imm() : nullptr;
	if (instance) {
		snapshot.playerPosition = instance->transform_imm().get_translate();
		snapshot.playerDirection = player->get_direction();
	}
	return snapshot;
}

void UndoManager::apply(const Snapshot& snapshot) {
	field->restore(snapshot.cells);
	if (!player) {
		return;
	}
	if (Reference<szg::WorldInstance> instance = player->get_world_instance_mut()) {
		instance->transform_mut().set_translate(snapshot.playerPosition);
	}
	player->set_direction(snapshot.playerDirection);
	// 掴んでいたセルは戻した盤面では無効。grippedBlockIndex を消すと PlayerGripState が Grip を抜ける
	PlayerContext& context = player->get_context_mut();
	context.grippedBlockIndex.reset();
	context.blockMoveResult.reset();
	context.verticalVelocity = 0.0f;
}
