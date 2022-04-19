#include <main.hpp>

namespace TemStream
{
Logger::Logger() : os(), streamLevel(Level::Info)
{
}
Logger::~Logger()
{
}
void Logger::flush()
{
	const auto s = os.str();
	Add(streamLevel, s, false);
	os.str("");
	os.clear();
	streamLevel = Level::Info;
}
void Logger::Add(const Level level, const String &s)
{
	Add(level, s, true);
}
void Logger::Add(const Log &log)
{
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
InMemoryLogger::InMemoryLogger() : Logger(), logs(), mutex()
{
}
InMemoryLogger::~InMemoryLogger()
{
}
void InMemoryLogger::Add(const Level level, const String &s, const bool)
{
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
} // namespace TemStream