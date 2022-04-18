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
	Add(streamLevel, s);
	os.str("");
	os.clear();
	streamLevel = Level::Info;
}
void Logger::Add(const Log &log)
{
	Add(log.first, log.second);
}
void Logger::Add(const Level level, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	Add(level, fmt, args);
	va_end(args);
}

ConsoleLogger::ConsoleLogger() : Logger()
{
}
ConsoleLogger::~ConsoleLogger()
{
}
void ConsoleLogger::Add(const Level level, const String &s)
{
	std::cout << level << ": " << s << std::endl;
}
void ConsoleLogger::Add(const Level level, const char *fmt, va_list args)
{
// Ignore trace if not debug
#if NDEBUG
	if (level >= Level::Trace)
	{
		return;
	}
#endif
	std::cout << level << ": ";
	std::vprintf(fmt, args);
}

InMemoryLogger::InMemoryLogger() : Logger(), logs(), mutex()
{
}
InMemoryLogger::~InMemoryLogger()
{
}
void InMemoryLogger::Add(const Level level, const String &s)
{
	logs.emplace_back(level, s);
}
void InMemoryLogger::Add(const Level level, const char *fmt, va_list args)
{
	LOCK(mutex);
	// Ignore trace if not debug
#if NDEBUG
	if (level >= Level::Trace)
	{
		return;
	}
#endif
	Log log;
	log.first = level;

	char buffer[KB(4)];
	const int len = snprintf(buffer, sizeof(buffer), fmt, args);
	log.second = String(buffer, buffer + len);

	logs.push_back(std::move(log));
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