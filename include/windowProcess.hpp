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

namespace TemStream
{
struct WindowProcess
{
	String name;
	void *data;
	union {
		int32_t id;
		uint32_t windowId;
	};

	WindowProcess() : name(), data(nullptr), id(0)
	{
	}
	WindowProcess(const String &name, const int32_t id) : name(name), data(nullptr), id(id)
	{
	}
	WindowProcess(const String &name, const uint32_t id) : name(name), data(nullptr), windowId(id)
	{
	}
	WindowProcess(String &&name, const int32_t id) : name(std::move(name)), data(nullptr), id(id)
	{
	}
	WindowProcess(String &&name, const uint32_t id) : name(std::move(name)), data(nullptr), windowId(id)
	{
	}
	~WindowProcess()
	{
	}

	bool operator==(const WindowProcess &p) const
	{
		return id == p.id && name == p.name;
	}
	bool operator!=(const WindowProcess &p) const
	{
		return !(*this == p);
	}
};
using WindowProcesses = Set<WindowProcess>;
} // namespace TemStream
namespace std
{
template <> struct hash<TemStream::WindowProcess>
{
	size_t operator()(const TemStream::WindowProcess &s) const
	{
		size_t value = hash<TemStream::String>()(s.name);
		TemStream::hash_combine(value, hash<int32_t>()(s.id));
		return value;
	}
};
} // namespace std