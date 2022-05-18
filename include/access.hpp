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
/**
 * @brief Defines which useres have access to a stream
 */
struct Access
{
	Set<String> members;
	bool banList;

	Access();
	~Access();

	/**
	 * @brief This checks if the user is granted accesss
	 *
	 * @param username
	 * @return True, if user has access
	 */
	bool isBanned(const String &username) const;

	/**
	 * Save for cereal serialization
	 *
	 * @param ar The archive
	 */
	template <class Archive> void save(Archive &ar) const
	{
		ar(members, banList);
	}

	/**
	 * Loading for cereal serialization
	 *
	 * @param ar The archive
	 */
	template <class Archive> void load(Archive &ar)
	{
		ar(members, banList);
	}

	friend std::ostream &operator<<(std::ostream &, const Access &);
};
} // namespace TemStream