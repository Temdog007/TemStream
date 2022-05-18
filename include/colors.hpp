/******************************************************************************
	Copyright (C) 2022 by Temitope Alaga <temdog007@yaoo.com>
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

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