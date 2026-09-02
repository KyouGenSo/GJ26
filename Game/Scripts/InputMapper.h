#pragma once

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Engine/Runtime/Input/InputHandler.h>

/// <summary>
/// 物理入力をゲーム内のアクションへ割り当てる
/// </summary>
template<typename Action, typename InputId>
class InputMapper {
public:
	/// <summary>
	/// アクションへ物理入力を追加
	/// </summary>
	void bind(Action action, InputId input);

	/// <summary>
	/// アクションから物理入力を削除
	/// </summary>
	bool unbind(Action action, InputId input);

	/// <summary>
	/// 指定したアクションの割り当てをすべて削除
	/// </summary>
	void clear(Action action);

	/// <summary>
	/// すべての割り当てを削除
	/// </summary>
	void clear();

	/// <summary>
	/// 入力状態を更新
	/// </summary>
	void update();

	bool trigger(Action action);
	bool press(Action action);
	bool release(Action action);

private:
	template<typename Query>
	bool query(Action action, Query&& queryFunction);

	void rebuild_handler();

private:
	std::unordered_map<Action, std::vector<InputId>> bindings;
	szg::InputHandler<InputId> inputHandler;
};

template<typename Action, typename InputId>
void InputMapper<Action, InputId>::bind(Action action, InputId input) {
	auto& inputs = bindings[action];
	if (std::find(inputs.begin(), inputs.end(), input) != inputs.end()) {
		return;
	}

	inputs.emplace_back(input);
	rebuild_handler();
}

template<typename Action, typename InputId>
bool InputMapper<Action, InputId>::unbind(Action action, InputId input) {
	auto binding = bindings.find(action);
	if (binding == bindings.end()) {
		return false;
	}

	auto& inputs = binding->second;
	auto target = std::find(inputs.begin(), inputs.end(), input);
	if (target == inputs.end()) {
		return false;
	}

	inputs.erase(target);
	if (inputs.empty()) {
		bindings.erase(binding);
	}
	rebuild_handler();
	return true;
}

template<typename Action, typename InputId>
void InputMapper<Action, InputId>::clear(Action action) {
	if (bindings.erase(action) == 0) {
		return;
	}
	rebuild_handler();
}

template<typename Action, typename InputId>
void InputMapper<Action, InputId>::clear() {
	bindings.clear();
	rebuild_handler();
}

template<typename Action, typename InputId>
void InputMapper<Action, InputId>::update() {
	inputHandler.update();
}

template<typename Action, typename InputId>
bool InputMapper<Action, InputId>::trigger(Action action) {
	return query(action, [this](InputId input) {
		return inputHandler.trigger(input);
	});
}

template<typename Action, typename InputId>
bool InputMapper<Action, InputId>::press(Action action) {
	return query(action, [this](InputId input) {
		return inputHandler.press(input);
	});
}

template<typename Action, typename InputId>
bool InputMapper<Action, InputId>::release(Action action) {
	return query(action, [this](InputId input) {
		return inputHandler.release(input);
	});
}

template<typename Action, typename InputId>
template<typename Query>
bool InputMapper<Action, InputId>::query(Action action, Query&& queryFunction) {
	auto binding = bindings.find(action);
	if (binding == bindings.end()) {
		return false;
	}

	return std::any_of(binding->second.begin(), binding->second.end(), std::forward<Query>(queryFunction));
}

template<typename Action, typename InputId>
void InputMapper<Action, InputId>::rebuild_handler() {
	std::vector<InputId> inputs;
	for (const auto& binding : bindings) {
		const auto& actionInputs = binding.second;
		for (InputId input : actionInputs) {
			if (std::find(inputs.begin(), inputs.end(), input) == inputs.end()) {
				inputs.emplace_back(input);
			}
		}
	}

	inputHandler.initialize(std::move(inputs));
}
