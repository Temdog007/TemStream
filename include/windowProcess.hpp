#pragma once

#include <main.hpp>

namespace TemStream
{
struct WindowProcess
{
	String name;
	union {
		int32_t id;
		uint32_t windowId;
	};

	WindowProcess() : name(), id(0)
	{
	}
	WindowProcess(const String &name, const int32_t id) : name(name), id(id)
	{
	}
	WindowProcess(const String &name, const uint32_t id) : name(name), windowId(id)
	{
	}
	WindowProcess(String &&name, const int32_t id) : name(std::move(name)), id(id)
	{
	}
	WindowProcess(String &&name, const uint32_t id) : name(std::move(name)), windowId(id)
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