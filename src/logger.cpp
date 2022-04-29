#include <main.hpp>

namespace TemStream
{
Logger::Logger() : os(), streamLevel(Level::Info), mutex()
{
}
Logger::~Logger()
{
}
void Logger::flush()
{
	LOCK(mutex);
	const auto s = os.str();
	Add(streamLevel, s, false);
	cleanSwap(os);
	streamLevel = Level::Info;
}
void Logger::Add(const Level level, const String &s)
{
	LOCK(mutex);
	Add(level, s, true);
}
void Logger::Add(const Log &log)
{
	LOCK(mutex);
	Add(log.first, log.second);
}
ConsoleLogger::ConsoleLogger() : Logger()
{
}
ConsoleLogger::~ConsoleLogger()
{
}
void ConsoleLogger::Add(const Level level, const String &s, const bool newLine)
{
	std::cout << level << ": " << s;
	if (newLine)
	{
		std::cout << std::endl;
	}
}
InMemoryLogger::InMemoryLogger() : Logger(), logs()
{
}
InMemoryLogger::~InMemoryLogger()
{
}
void InMemoryLogger::Add(const Level level, const String &s, const bool)
{
	LOCK(mutex);
	logs.emplace_back(level, s);
}
void InMemoryLogger::viewLogs(const std::function<void(const Log &)> &f)
{
	LOCK(mutex);
	for (const auto &log : logs)
	{
		f(log);
	}
}
void InMemoryLogger::clear()
{
	LOCK(mutex);
	logs.clear();
}
size_t InMemoryLogger::size()
{
	LOCK(mutex);
	return logs.size();
}
} // namespace TemStream