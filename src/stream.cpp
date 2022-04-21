#include <main.hpp>

namespace TemStream
{
Stream::Stream() : access(), source(), type(UINT32_MAX)
{
}
Stream::Stream(const Message::Source &s, const uint32_t u) : access(), source(s), type(u)
{
}
Stream::Stream(const Stream &s) : access(s.access), source(s.source), type(s.type)
{
}
Stream::Stream(Stream &&s) : access(std::move(s.access)), source(std::move(s.source)), type(std::move(s.type))
{
}
Stream::~Stream()
{
}
Stream &Stream::operator=(const Stream &s)
{
	access = s.access;
	source = s.source;
	type = s.type;
	return *this;
}
Stream &Stream::operator=(Stream &&s)
{
	access = std::move(s.access);
	source = std::move(s.source);
	type = std::move(s.type);
	return *this;
}
Stream::Access::Access() : users(), isBanList(true)
{
}
Stream::Access::Access(Set<String> &&set, const bool b) : users(std::move(set)), isBanList(b)
{
}
Stream::Access::~Access()
{
}
bool Stream::Access::allowed(const String &name) const
{
	if (isBanList)
	{
		return users.find(name) != users.end();
	}
	else
	{
		return users.find(name) == users.end();
	}
}
} // namespace TemStream