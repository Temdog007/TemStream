#pragma once

#include <main.hpp>

namespace TemStream
{
template <typename T> class ConcurrentQueue
{
  private:
	Queue<T> queue;
	std::condition_variable cv;
	std::mutex mutex;

	std::unique_lock<std::mutex> lock()
	{
		return std::unique_lock<std::mutex>(mutex);
	}

  public:
	ConcurrentQueue() : queue(), cv(), mutex()
	{
	}
	ConcurrentQueue(const ConcurrentQueue &) = delete;
	ConcurrentQueue(ConcurrentQueue &&) = delete;
	~ConcurrentQueue()
	{
	}

	std::optional<T> tryPop()
	{
		auto lck = lock();
		if (queue.empty())
		{
			return std::nullopt;
		}
		T t = std::move(queue.front());
		queue.pop();
		return t;
	}

	T pop()
	{
		auto lck = lock();
		while (queue.empty())
		{
			cv.wait(lck);
		}
		T t = std::move(queue.front());
		queue.pop();
		return t;
	}

	template <typename _Rep, typename _Period>
	std::optional<T> pop(const std::chrono::duration<_Rep, _Period> &maxWaitTime)
	{
		auto lck = lock();
		while (queue.empty())
		{
			if (cv.wait_for(lck, maxWaitTime) == std::cv_status::timeout)
			{
				return std::nullopt;
			}
		}
		T t = std::move(queue.front());
		queue.pop();
		return t;
	}

	void push(const T &t)
	{
		auto lck = lock();
		queue.push(t);
		cv.notify_all();
	}

	void push(T &&t)
	{
		auto lck = lock();
		queue.push(std::move(t));
		cv.notify_all();
	}

	template <typename... _Args> void emplace(_Args &&...__args)
	{
		auto lck = lock();
		queue.emplace_back(std::forward<_Args>(__args)...);
		cv.notify_all();
	}
};
} // namespace TemStream