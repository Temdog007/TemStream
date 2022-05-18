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
Guid::Guid() : longs{0, 0}
{
}
Guid::Guid(const uint64_t a, const uint64_t b) : longs{a, b}
{
}
Guid::~Guid()
{
}
Guid Guid::random()
{
	Guid g;
	for (int i = 0; i < 4; ++i)
	{
		g.ints[i] = rand();
	}
	return g;
}
bool Guid::operator==(const Guid &g) const
{
	return longs[0] == g.longs[0] && longs[1] == g.longs[1];
}
std::ostream &operator<<(std::ostream &os, const Guid &g)
{
	const auto &guid = g.data;
	os << std::hex << std::setw(8) << std::setfill('0') << guid.data1;
	os << '-';
	os << std::hex << std::setw(4) << std::setfill('0') << guid.data2;
	os << '-';
	os << std::hex << std::setw(4) << std::setfill('0') << guid.data3;
	os << '-';
	os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.data4[0]);
	os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.data4[1]);
	os << '-';
	os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.data4[2]);
	os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.data4[3]);
	os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.data4[4]);
	os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.data4[5]);
	os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.data4[6]);
	os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.data4[7]);
	os << std::dec;
	return os;
}
} // namespace TemStream