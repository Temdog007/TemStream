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
template <typename Key, typename Value> class ConcurrentMap
{
  private:
	Map<Key, Value> map;
	Mutex &mutex;

  public:
	ConcurrentMap(Mutex &mutex) : map(), mutex(mutex)
	{
	}
	~ConcurrentMap()
	{
	}

	template <typename... _Args> bool add(const Key &key, _Args &&...__args)
	{
		LOCK(mutex);
		auto [iter, result] = map.try_emplace(key, std::forward<_Args>(__args)...);
		return result;
	}
	template <typename... _Args> bool add(Key &&key, _Args &&...__args)
	{
		LOCK(mutex);
		auto [iter, result] = map.try_emplace(std::move(key), std::forward<_Args>(__args)...);
		return result;
	}

	bool remove(const Key &key)
	{
		LOCK(mutex);
		return map.erase(key) != 0;
	}
	void clear()
	{
		LOCK(mutex);
		map.clear();
	}

	size_t size()
	{
		LOCK(mutex);
		return map.size();
	}

	bool empty()
	{
		LOCK(mutex);
		return map.empty();
	}

	bool use(const Key &key, const std::function<void(Value &)> &func)
	{
		LOCK(mutex);
		auto iter = map.find(key);
		if (iter == map.end())
		{
			return false;
		}
		else
		{
			func(iter->second);
			return true;
		}
	}

	Value find(const Key &key, Value defaultValue)
	{
		LOCK(mutex);
		auto iter = map.find(key);
		if (iter == map.end())
		{
			return defaultValue;
		}
		return iter->second;
	}

	void forEach(const std::function<void(const Key &, Value &)> &func)
	{
		LOCK(mutex);
		for (auto pair : map)
		{
			func(pair.first, pair.second);
		}
	}
	size_t removeIfNot(const std::function<bool(const Key &, Value &)> &func)
	{
		LOCK(mutex);
		size_t count = 0;
		for (auto iter = map.begin(); iter != map.end();)
		{
			if (func(iter->first, iter->second))
			{
				++iter;
			}
			else
			{
				iter = map.erase(iter);
				++count;
			}
		}
		return count;
	}
};
} // namespace TemStream