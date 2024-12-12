#pragma once
#include <map>
#include "glCommon.h"

enum KeyAction {
	KeyAction_Save,
	KeyAction_Skip,
	KeyAction_Next,
	KeyAction_Previous,
	KeyAction_Reset,
	KeyAction_ToggleHideSkipped,
	KeyAction_ToggleLockMovement
};

/*
Struct for configurable settings.
*/
struct Config {
	std::map<int, KeyAction> keyToAction{};

	Config() {
		keyToAction.emplace(GLFW_KEY_SPACE, KeyAction_Save);
		keyToAction.emplace(GLFW_KEY_ENTER, KeyAction_Save);
		keyToAction.emplace(GLFW_KEY_1, KeyAction_Save);

		keyToAction.emplace(GLFW_KEY_BACKSPACE, KeyAction_Skip);
		keyToAction.emplace(GLFW_KEY_X, KeyAction_Skip);
		keyToAction.emplace(GLFW_KEY_2, KeyAction_Skip);

		keyToAction.emplace(GLFW_KEY_DOWN, KeyAction_Next);
		keyToAction.emplace(GLFW_KEY_S, KeyAction_Next);

		keyToAction.emplace(GLFW_KEY_UP, KeyAction_Previous);
		keyToAction.emplace(GLFW_KEY_W, KeyAction_Previous);

		keyToAction.emplace(GLFW_KEY_R, KeyAction_Reset);
		keyToAction.emplace(GLFW_KEY_3, KeyAction_Reset);

		keyToAction.emplace(GLFW_KEY_H, KeyAction_ToggleHideSkipped);

		keyToAction.emplace(GLFW_KEY_L, KeyAction_ToggleLockMovement);
	}
};
