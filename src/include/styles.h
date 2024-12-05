#pragma once
#include "imgui.h"

namespace Colors {
	const ImVec4 transparent		= ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

	const ImVec4 text				= ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	const ImVec4 textHint			= ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
	const ImVec4 textDisabled		= ImVec4(0.24f, 0.24f, 0.24f, 1.00f);

	const ImVec4 green				= ImVec4(0.27f, 0.68f, 0.28f, 1.00f);
	const ImVec4 yellow				= ImVec4(0.65f, 0.56f, 0.20f, 1.00f);

	const ImVec4 gray1				= ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
	const ImVec4 gray2				= ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	const ImVec4 gray3				= ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	const ImVec4 gray3Transparent	= ImVec4(0.25f, 0.25f, 0.25f, 0.50f);
	const ImVec4 gray4				= ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
	const ImVec4 gray4Transparent	= ImVec4(0.40f, 0.40f, 0.40f, 0.50f);
}

void setGlobalStyles() {
	ImGuiStyle& style = ImGui::GetStyle();

	style.Colors[ImGuiCol_WindowBg] = Colors::gray1;
	style.Colors[ImGuiCol_MenuBarBg] = Colors::gray1;
	style.Colors[ImGuiCol_Border] = Colors::gray2;
	
	style.Colors[ImGuiCol_Header] = Colors::gray2;
	style.Colors[ImGuiCol_HeaderHovered] = Colors::gray2;
	style.Colors[ImGuiCol_HeaderActive] = Colors::gray2;

	style.Colors[ImGuiCol_Button] = Colors::gray3;
	style.Colors[ImGuiCol_ButtonHovered] = Colors::gray4;
	style.Colors[ImGuiCol_ButtonActive] = Colors::gray4;

	// "Frame" colors apply to the background of checkboxes and slider, among other widgets
	style.Colors[ImGuiCol_FrameBg] = Colors::gray3;
	style.Colors[ImGuiCol_FrameBgHovered] = Colors::gray3;
	style.Colors[ImGuiCol_FrameBgActive] = Colors::gray3;

	style.Colors[ImGuiCol_CheckMark] = Colors::gray4;
	style.Colors[ImGuiCol_SliderGrab] = Colors::gray4;
	style.Colors[ImGuiCol_SliderGrabActive] = Colors::gray4;

	// Applies to the resizable borders within dockspaces
	style.Colors[ImGuiCol_ResizeGrip] = Colors::gray2;
	style.Colors[ImGuiCol_ResizeGripHovered] = Colors::gray3;
	style.Colors[ImGuiCol_ResizeGripActive] = Colors::gray3;

}
