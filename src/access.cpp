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