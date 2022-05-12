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
	(*logger)(Logger::Info) << name << " took " << std::chrono::duration_cast<std::chrono::milliseconds>(t).count()
							<< " milliseconds" << std::endl;
}
} // namespace TemStream