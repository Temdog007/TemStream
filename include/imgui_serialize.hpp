#pragma once

#include <cereal/cereal.hpp>
#include <imgui.h>

template <class Archive> static inline void serialize(Archive &ar, ImVec2 &v)
{
	ar(cereal ::make_nvp("x", v.x), cereal::make_nvp("y", v.y));
}

template <class Archive> static inline void serialize(Archive &ar, ImVec4 &v)
{
	ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z), cereal::make_nvp("w", v.w));
}

#define SAVE_STYLE_ARG(name) cereal::make_nvp(#name, style.name)

template <class Archive> void serialize(Archive &ar, ImGuiStyle &style)
{
	ar(SAVE_STYLE_ARG(Alpha), SAVE_STYLE_ARG(DisabledAlpha), SAVE_STYLE_ARG(WindowPadding),
	   SAVE_STYLE_ARG(WindowRounding), SAVE_STYLE_ARG(WindowBorderSize), SAVE_STYLE_ARG(WindowMinSize),
	   SAVE_STYLE_ARG(WindowTitleAlign), SAVE_STYLE_ARG(WindowMenuButtonPosition), SAVE_STYLE_ARG(ChildRounding),
	   SAVE_STYLE_ARG(ChildBorderSize), SAVE_STYLE_ARG(PopupRounding), SAVE_STYLE_ARG(PopupBorderSize),
	   SAVE_STYLE_ARG(FramePadding), SAVE_STYLE_ARG(FrameRounding), SAVE_STYLE_ARG(FrameBorderSize),
	   SAVE_STYLE_ARG(ItemSpacing), SAVE_STYLE_ARG(ItemInnerSpacing), SAVE_STYLE_ARG(CellPadding),
	   SAVE_STYLE_ARG(TouchExtraPadding), SAVE_STYLE_ARG(IndentSpacing), SAVE_STYLE_ARG(ColumnsMinSpacing),
	   SAVE_STYLE_ARG(ScrollbarSize), SAVE_STYLE_ARG(ScrollbarRounding), SAVE_STYLE_ARG(GrabMinSize),
	   SAVE_STYLE_ARG(GrabRounding), SAVE_STYLE_ARG(LogSliderDeadzone), SAVE_STYLE_ARG(TabRounding),
	   SAVE_STYLE_ARG(TabBorderSize), SAVE_STYLE_ARG(TabMinWidthForCloseButton), SAVE_STYLE_ARG(ColorButtonPosition),
	   SAVE_STYLE_ARG(ButtonTextAlign), SAVE_STYLE_ARG(SelectableTextAlign), SAVE_STYLE_ARG(DisplayWindowPadding),
	   SAVE_STYLE_ARG(DisplaySafeAreaPadding), SAVE_STYLE_ARG(MouseCursorScale), SAVE_STYLE_ARG(AntiAliasedLines),
	   SAVE_STYLE_ARG(AntiAliasedLinesUseTex), SAVE_STYLE_ARG(AntiAliasedFill), SAVE_STYLE_ARG(CurveTessellationTol),
	   SAVE_STYLE_ARG(CircleTessellationMaxError));

	std::array<ImVec4, ImGuiCol_COUNT> colors;
	std::copy(style.Colors, style.Colors + ImGuiCol_COUNT, colors.begin());
	ar(CEREAL_NVP(colors));
	std::copy(colors.begin(), colors.end(), style.Colors);
}
