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
	Logger::Add(streamLevel, s);
	os.str("");
	os.clear();
	streamLevel = Level::Info;
}
void Logger::Add(const Level level, const String &s)
{
	Add(level, "%s", s.c_str());
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

	const int length = std::vsnprintf(nullptr, 0, fmt, args) + 1;
	char *c = new char[length];
	std::vsnprintf(c, length, fmt, args);
	log.second = String(c, c + length);
	delete[] c;

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