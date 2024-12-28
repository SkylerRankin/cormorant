#pragma once
#include <map>
#include "glCommon.h"

namespace Config {
	enum KeyAction {
		KeyAction_Save = 0,
		KeyAction_Skip = 1,
		KeyAction_Next = 2,
		KeyAction_Previous = 3,
		KeyAction_Reset = 4,
		KeyAction_ToggleHideSkipped = 5,
		KeyAction_ToggleLockMovement = 6,
		KeyAction_Count = 7
	};

	struct Config {
		std::map<int, KeyAction> keyToAction{};
		unsigned int cacheCapacity = 10;
		unsigned int cacheForwardPreload = 3;
		unsigned int cacheBackwardPreload = 1;

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

		void update(const Config& source) {
			for (const auto& e : source.keyToAction) {
				keyToAction.emplace(e.first, e.second);
			}

			cacheCapacity = source.cacheCapacity;
			cacheForwardPreload = source.cacheForwardPreload;
			cacheBackwardPreload = source.cacheBackwardPreload;
		}
	};

	void loadConfig(Config& config);
	void saveConfig(const Config& config);
}
