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