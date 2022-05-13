#pragma once

#include <main.hpp>

namespace Colors
{
extern const ImVec4 Yellow;
extern const ImVec4 Cyan;
extern const ImVec4 Magenta;

extern const ImVec4 DarkYellow;
extern const ImVec4 DarkCyan;
extern const ImVec4 DarkMagenta;

extern const ImVec4 Black;
extern const ImVec4 White;

extern const ImVec4 Red;
extern const ImVec4 DarkRed;
extern const ImVec4 Green;
extern const ImVec4 Blue;

extern const ImVec4 Lime;

extern const ImVec4 &GetGreen(const bool isLight);
extern const ImVec4 &GetYellow(const bool isLight);
extern const ImVec4 &GetRed(const bool isLight);

extern const ImVec4 &GetCyan(const bool isLight);

extern void StyleDeepDark(ImGuiStyle &style);
extern void StyleRed(ImGuiStyle &style);
extern void StyleGreen(ImGuiStyle &style);
extern void StyleGold(ImGuiStyle &style);
} // namespace Colors