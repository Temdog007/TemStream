#pragma once

#include <main.hpp>

namespace TemStream
{
class Stream
{
  public:
	struct Access
	{
		Set<String> users;

		// If true, special list is a list of users who are banned
		// Else, special list is the only users allowed access
		bool isBanList;

		Access();
		Access(Set<String> &&, bool);
		~Access();

		template <class Archive> void save(Archive &ar) const
		{
			ar(users, isBanList);
		}
		template <class Archive> void load(Archive &ar)
		{
			ar(users, isBanList);
		}

		bool allowed(const String &) const;
	};

  private:
	Access access;
	Message::Source source;
	uint32_t type;

  public:
	Stream();
	Stream(const Message::Source &, uint32_t);
	Stream(const Stream &);
	Stream(Stream &&);
	~Stream();

	Stream &operator=(const Stream &);
	Stream &operator=(Stream &&);

	template <class Archive> void save(Archive &ar) const
	{
		ar(access, source, type);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(access, source, type);
	}

	uint32_t getType() const
	{
		return type;
	}

	const Message::Source &getSource() const
	{
		return source;
	}

	const Access &getAccess() const
	{
		return access;
	}

	void setAccess(const Access &a)
	{
		access = a;
	}

	void setAccess(Access &&a)
	{
		access = std::move(a);
	}
};
} // namespace TemStream