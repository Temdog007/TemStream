#pragma once

#include <main.hpp>

namespace TemStream
{
#define LOG_FUNC(X)                                                                                                    \
	void Add##X(const char *fmt, ...)                                                                                  \
	{                                                                                                                  \
		va_list args;                                                                                                  \
		va_start(args, fmt);                                                                                           \
		Add(Level::X, fmt, args);                                                                                      \
		va_end(args);                                                                                                  \
	}                                                                                                                  \
	void Add##X(const String &s)                                                                                       \
	{                                                                                                                  \
		Add(Level::X, s);                                                                                              \
	}

class Logger
{
  public:
	typedef std::ostream &(*ManipFn)(std::ostream &);
	typedef std::ios_base &(*FlagsFn)(std::ios_base &);
	enum Level
	{
		Error,
		Warning,
		Info,
		Trace
	};

  private:
	StringStream os;
	Level streamLevel;

  protected:
	const StringStream &getStream()
	{
		return os;
	}
	Level getStreamLevel() const
	{
		return streamLevel;
	}

  public:
	using Log = std::pair<Level, String>;
	Logger();
	virtual ~Logger();

	virtual void Add(Level, const char *, va_list) = 0;
	void Add(Level, const char *, ...);
	virtual void Add(Level, const String &) = 0;
	void Add(const Log &);

	LOG_FUNC(Error);
	LOG_FUNC(Warning);
	LOG_FUNC(Info);
	LOG_FUNC(Trace);

	template <typename T> Logger &operator<<(const T &data)
	{
		os << data;
		return *this;
	}

	// Source:
	// https://stackoverflow.com/questions/40273809/how-to-write-iostream-like-interface-to-logging-library/40424272#40424272
	Logger &operator<<(ManipFn manip) /// endl, flush, setw, setfill, etc.
	{
		manip(os);

		if (manip == static_cast<ManipFn>(std::flush) || manip == static_cast<ManipFn>(std::endl))
		{
			this->flush();
		}

		return *this;
	}

	Logger &operator<<(FlagsFn manip) /// setiosflags, resetiosflags
	{
		manip(os);
		return *this;
	}

	Logger &operator()(const Level level)
	{
		streamLevel = level;
		return *this;
	}

	void flush();
};
static inline std::ostream &operator<<(std::ostream &os, const Logger::Level lvl)
{
	switch (lvl)
	{
	case Logger::Level::Error:
		os << "Error";
		break;
	case Logger::Level::Warning:
		os << "Warning";
		break;
	case Logger::Level::Info:
		os << "Info";
		break;
	case Logger::Level::Trace:
		os << "Trace";
		break;
	default:
		break;
	}
	return os;
}
class ConsoleLogger : public Logger
{
  public:
	ConsoleLogger();
	~ConsoleLogger();
	void Add(Level, const char *, va_list) override;
	void Add(Level, const String &) override;
};

class InMemoryLogger : public Logger
{
  private:
	List<Log> logs;
	Mutex mutex;

  public:
	InMemoryLogger();
	virtual ~InMemoryLogger();

	virtual void Add(Level, const char *, va_list) override;
	void Add(Level, const String &) override;

	void viewLogs(const std::function<void(const Log &)> &);
};

extern std::unique_ptr<Logger> logger;
} // namespace TemStream