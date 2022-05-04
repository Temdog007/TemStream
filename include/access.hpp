#pragma once

#include <main.hpp>

namespace TemStream
{
struct Access
{
	Set<String> members;
	bool banList;

	Access();
	~Access();

	bool isBanned(const String &) const;

	template <class Archive> void save(Archive &ar) const
	{
		ar(members, banList);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(members, banList);
	}

	friend std::ostream &operator<<(std::ostream &, const Access &);
};
} // namespace TemStream