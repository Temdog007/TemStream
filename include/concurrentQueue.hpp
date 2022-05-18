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

	static void clearQueue(Queue<T> &queue)
	{
		Queue<T> empty;
		empty.swap(queue);
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

	template <typename _Rep, typename _Period> T tryPop(const std::chrono::duration<_Rep, _Period> &maxWaitTime)
	{
		auto lck = lock();
		while (queue.empty())
		{
			if (cv.wait_for(lck, maxWaitTime) == std::cv_status::timeout)
			{
				return nullptr;
			}
		}
		auto t = std::move(queue.front());
		queue.pop();
		return t;
	}

	void flush(const std::function<void(T &&)> &func)
	{
		auto lck = lock();
		while (!queue.empty())
		{
			T t = std::move(queue.front());
			queue.pop();
			func(std::move(t));
		}
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
		queue.emplace(std::forward<_Args>(__args)...);
		cv.notify_all();
	}

	void use(const std::function<void(Queue<T> &)> &f)
	{
		auto lck = lock();
		f(queue);
		cv.notify_all();
	}

	std::optional<size_t> clearIfGreaterThan(const size_t size)
	{
		auto lck = lock();
		std::optional<size_t> rval = std::nullopt;
		if (queue.size() > size)
		{
			rval = queue.size();
			clearQueue(queue);
		}
		cv.notify_all();
		return rval;
	}

	template <typename R> R use(const std::function<R(Queue<T> &)> &f)
	{
		auto lck = lock();
		R r = f(queue);
		cv.notify_all();
		return r;
	}

	size_t size()
	{
		auto lck = lock();
		return queue.size();
	}

	bool empty()
	{
		auto lck = lock();
		return queue.empty();
	}

	void clear()
	{
		auto lck = lock();
		clearQueue(queue);
		cv.notify_all();
	}
};
} // namespace TemStream