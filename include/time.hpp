#pragma once

#include <main.hpp>

namespace TemStream
{
class Time
{
  private:
	String name;
	const std::chrono::_V2::system_clock::time_point start;

  public:
	Time(const String &);
	Time() = delete;
	Time(const Time &) = delete;
	Time(Time &&) = delete;
	~Time();
};
} // namespace TemStream