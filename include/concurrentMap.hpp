#pragma once

#include <main.hpp>

namespace TemStream
{
template <typename Key, typename Value> class ConcurrentMap
{
  private:
	Map<Key, Value> map;
	Mutex mutex;

  public:
	ConcurrentMap() : map(), mutex()
	{
	}
	ConcurrentMap(const ConcurrentMap &) = delete;
	ConcurrentMap(ConcurrentMap &&) = delete;
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

	std::optional<Value> tryFind(const Key &key)
	{
		if (auto lck = std::unique_lock{mutex, std::try_to_lock})
		{
			auto iter = map.find(key);
			if (iter == map.end())
			{
				return std::nullopt;
			}
			return iter->second;
		}
		return std::nullopt;
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