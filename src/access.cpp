#include <main.hpp>

namespace TemStream
{
Access::Access() : members(), banList(true)
{
}
Access::~Access()
{
}
bool Access::isBanned(const String &member) const
{
	auto iter = members.find(member);
	if (banList)
	{
		return iter != members.end();
	}
	else
	{
		return iter == members.end();
	}
}
std::ostream &operator<<(std::ostream &os, const Access &access)
{
	if (access.banList)
	{
		os << "Ban list: [ ";
	}
	else
	{
		os << "Accepted list: [ ";
	}
	for (const auto &member : access.members)
	{
		os << member << ", ";
	}
	os << ']';
	return os;
}
} // namespace TemStream