#pragma once

#include <main.hpp>

namespace TemStream
{
class Time
{
  private:
	String name;
	const TimePoint start;

  public:
	Time(const String &);
	Time() = delete;
	Time(const Time &) = delete;
	Time(Time &&) = delete;
	~Time();
};
} // namespace TemStream