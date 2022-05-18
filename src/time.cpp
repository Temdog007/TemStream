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

using namespace std::chrono;

namespace TemStream
{
Time::Time(const String &name) : name(name), start(system_clock::now())
{
}
Time::~Time()
{
	const auto t = system_clock::now() - start;
	(*logger)(Logger::Level::Info) << name << " took "
								   << std::chrono::duration_cast<std::chrono::milliseconds>(t).count()
								   << " milliseconds" << std::endl;
}
} // namespace TemStream