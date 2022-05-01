#pragma once

#include <main.hpp>

namespace TemStream
{
struct Guid
{
	struct Data
	{
		uint32_t data1;
		uint16_t data2;
		uint16_t data3;
		uint8_t data4[8];
	};
	union {
		uint8_t bytes[16];
		uint16_t shorts[8];
		int32_t ints[4];
		uint64_t longs[2];
		Data data;
	};

	Guid();
	Guid(uint64_t, uint64_t);
	~Guid();

	bool operator==(const Guid &) const;
	bool operator!=(const Guid &g) const
	{
		return !(*this == g);
	}

	static Guid random();

	template <class Archive> void save(Archive &ar) const
	{
		ar(longs[0], longs[1]);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(longs[0], longs[1]);
	}

	friend std::ostream &operator<<(std::ostream &os, const Guid &g);
};
} // namespace TemStream