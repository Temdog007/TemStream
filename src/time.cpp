#include <main.hpp>

using namespace std::chrono;

namespace TemStream
{
Time::Time(const String &name) : name(name), start(high_resolution_clock::now())
{
}
Time::~Time()
{
	const duration<double, std::milli> t = high_resolution_clock::now() - start;
	(*logger)(Logger::Info) << name << " took " << t.count() << " milliseconds" << std::endl;
}
} // namespace TemStream