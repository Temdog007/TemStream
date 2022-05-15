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