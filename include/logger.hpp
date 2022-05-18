/******************************************************************************
	Copyright (C) 2022 by Temitope Alaga <temdog007@yaoo.com>
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include <main.hpp>

namespace TemStream
{
#define LOG_FUNC(X)                                                                                                    \
	void Add##X(const String &s)                                                                                       \
	{                                                                                                                  \
		Add(Level::X, s);                                                                                              \
	}

class Logger
{
  public:
	typedef std::ostream &(*ManipFn)(std::ostream &);
	typedef std::ios_base &(*FlagsFn)(std::ios_base &);
	enum class Level
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
	Mutex mutex;

	virtual void Add(Level, const String &, bool) = 0;
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

	void Add(Level, const String &);
	void Add(const Log &);

	LOG_FUNC(Error);
	LOG_FUNC(Warning);
	LOG_FUNC(Info);
	LOG_FUNC(Trace);

	Logger &operator<<(const uint64_t data)
	{
		LOCK(mutex);
		os << data;
		return *this;
	}

	template <typename T> Logger &operator<<(const T &data)
	{
		LOCK(mutex);
		os << data;
		return *this;
	}

	// Source:
	// https://stackoverflow.com/questions/40273809/how-to-write-iostream-like-interface-to-logging-library/40424272#40424272
	Logger &operator<<(ManipFn manip) /// endl, flush, setw, setfill, etc.
	{
		LOCK(mutex);
		manip(os);

		if (manip == static_cast<ManipFn>(std::flush) || manip == static_cast<ManipFn>(std::endl))
		{
			this->flush();
		}

		return *this;
	}

	Logger &operator<<(FlagsFn manip) /// setiosflags, resetiosflags
	{
		LOCK(mutex);
		manip(os);
		return *this;
	}

	Logger &operator()(const Level level)
	{
		LOCK(mutex);
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
  protected:
	void Add(Level, const String &, bool) override;

  public:
	ConsoleLogger();
	~ConsoleLogger();
};

class InMemoryLogger : public Logger
{
  private:
	List<Log> logs;

  protected:
	virtual void Add(Level, const String &, bool) override;

  public:
	InMemoryLogger();
	virtual ~InMemoryLogger();

	void viewLogs(const std::function<void(const Log &)> &);
	void clear();
	size_t size();
};

extern unique_ptr<Logger> logger;
} // namespace TemStream