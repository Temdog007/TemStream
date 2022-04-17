#pragma once

#include <main.hpp>

namespace TemStream
{
enum PlacementPolicy
{
	First,
	Best
};
struct FreeListNode
{
	size_t blockSize;
	FreeListNode *next;

	static void insert(FreeListNode *&head, FreeListNode *previous, FreeListNode *newNode)
	{
		if (previous == nullptr)
		{
			newNode->next = head;
			head = newNode;
		}
		else
		{
			newNode->next = previous->next;
			previous->next = newNode;
		}
	}
	static void remove(FreeListNode *&head, FreeListNode *previous, FreeListNode *deleteNode)
	{
		if (previous == nullptr)
		{
			head = deleteNode->next;
		}
		else
		{
			previous->next = deleteNode->next;
		}
	}
};
template <class T> class Allocator;
class AllocatorData
{
  private:
	Mutex mutex;
	FreeListNode *list;
	void *data;
	size_t used;
	size_t len;
	PlacementPolicy policy;

	template <class T> friend class Allocator;

	void reset()
	{
		FreeListNode *first = reinterpret_cast<FreeListNode *>(data);
		first->blockSize = len;
		first->next = nullptr;
		used = 0;
		list = nullptr;
		FreeListNode::insert(list, nullptr, first);
	}

	void close()
	{
		if (data != nullptr)
		{
			free(data);
			data = nullptr;
		}
	}

  public:
	AllocatorData() : mutex(), list(nullptr), data(nullptr), used(0), len(0), policy(PlacementPolicy::Best)
	{
	}
	AllocatorData(const AllocatorData &) = delete;
	AllocatorData(AllocatorData &&) = delete;

	~AllocatorData()
	{
		close();
	}

	void init(const size_t len, PlacementPolicy policy = PlacementPolicy::Best)
	{
		close();

		list = nullptr;
		used = 0;
		this->len = len;
		data = malloc(len);
		this->policy = policy;
		reset();
	}
};
extern AllocatorData globalAllocatorData;
template <class T> class Allocator
{
  public:
	typedef T value_type;

  private:
	AllocatorData &ad;

	void coalescence(FreeListNode *previousNode, FreeListNode *freeNode)
	{
		if (freeNode->next != nullptr &&
			reinterpret_cast<size_t>(freeNode) + freeNode->blockSize == reinterpret_cast<size_t>(freeNode->next))
		{
			freeNode->blockSize += freeNode->next->blockSize;
			FreeListNode::remove(ad.list, freeNode, freeNode->next);
		}
		if (previousNode != nullptr &&
			reinterpret_cast<size_t>(previousNode) + previousNode->blockSize == reinterpret_cast<size_t>(freeNode))
		{
			previousNode->blockSize += freeNode->blockSize;
			FreeListNode::remove(ad.list, previousNode, freeNode);
		}
	}

	void find(const size_t size, FreeListNode *&previousNode, FreeListNode *&foundNode)
	{
		switch (ad.policy)
		{
		case PlacementPolicy::First:
			findFirst(size, previousNode, foundNode);
			break;
		case PlacementPolicy::Best:
			findBest(size, previousNode, foundNode);
			break;
		default:
			break;
		}
	}

	void findFirst(const size_t size, FreeListNode *&previousNode, FreeListNode *&foundNode)
	{
		FreeListNode *it = ad.list;
		FreeListNode *prev = nullptr;
		while (it != nullptr)
		{
			if (it->blockSize >= size)
			{
				break;
			}
			prev = it;
			it = it->next;
		}
		previousNode = prev;
		foundNode = it;
	}

	void findBest(const size_t size, FreeListNode *&previousNode, FreeListNode *&foundNode)
	{
		size_t smallestDiff = SIZE_MAX;
		FreeListNode *bestBlock = nullptr;
		FreeListNode *bestPrevBlock = nullptr;
		FreeListNode *it = ad.list;
		FreeListNode *prev = nullptr;
		while (it != nullptr)
		{
			const size_t currentDiff = it->blockSize - size;
			if (it->blockSize >= size && currentDiff < smallestDiff)
			{
				bestBlock = it;
				bestPrevBlock = prev;
				smallestDiff = currentDiff;
			}
			prev = it;
			it = it->next;
		}
		previousNode = bestPrevBlock;
		foundNode = bestBlock;
	}

  public:
	Allocator() noexcept : ad(globalAllocatorData)
	{
	}
	Allocator(AllocatorData &ad) noexcept : ad(ad)
	{
	}
	~Allocator()
	{
	}

	template <class U> Allocator(const Allocator<U> &u) noexcept : ad(u.ad)
	{
	}
	template <class U> bool operator==(const Allocator<U> &) const noexcept
	{
		return true;
	}
	template <class U> bool operator!=(const Allocator<U> &) const noexcept
	{
		return false;
	}

	T *allocate(const size_t n);
	void deallocate(T *const p, size_t);
};

#if __ANDROID__
#define ALLOCATOR_ALIGNMENT 16
#else
#define ALLOCATOR_ALIGNMENT (2 * sizeof(void *))
#endif
template <class T> T *Allocator<T>::allocate(const size_t requestedSize)
{
	if (requestedSize == 0)
	{
		return nullptr;
	}

	LOCK(ad.mutex);

	size_t size = std::max(requestedSize, ALLOCATOR_ALIGNMENT);
	size += ALLOCATOR_ALIGNMENT - (size % ALLOCATOR_ALIGNMENT);
	const size_t allocateSize = size + sizeof(FreeListNode);

	FreeListNode *affectedNode = nullptr;
	FreeListNode *previousNode = nullptr;
	find(allocateSize, previousNode, affectedNode);
	if (affectedNode == nullptr)
	{
		throw std::bad_alloc();
	}

	const size_t rest = affectedNode->blockSize - allocateSize;
	if (rest > 0)
	{
		FreeListNode *newFreeNode =
			reinterpret_cast<FreeListNode *>(reinterpret_cast<size_t>(affectedNode) + allocateSize);
		newFreeNode->blockSize = rest;
		newFreeNode->next = nullptr;
		FreeListNode::insert(ad.list, affectedNode, newFreeNode);
	}

	FreeListNode::remove(ad.list, previousNode, affectedNode);
	affectedNode->blockSize = allocateSize;
	affectedNode->next = nullptr;

	ad.used += allocateSize;

	const size_t dataAddress = reinterpret_cast<size_t>(affectedNode) + sizeof(FreeListNode);
	T *ptr = reinterpret_cast<T *>(dataAddress);
	return ptr;
}
template <class T> void Allocator<T>::deallocate(T *const ptr, const size_t)
{
	if (ptr == nullptr)
	{
		return;
	}

	LOCK(ad.mutex);

	const size_t currentAddress = reinterpret_cast<size_t>(ptr);
	const size_t headerAddress = currentAddress - sizeof(FreeListNode);

	FreeListNode *freeNode = reinterpret_cast<FreeListNode *>(headerAddress);
	freeNode->next = nullptr;

	FreeListNode *it = ad.list;
	FreeListNode *prev = nullptr;
	while (it != nullptr)
	{
		if (freeNode < it)
		{
			FreeListNode::insert(ad.list, prev, freeNode);
			break;
		}
		prev = it;
		it = it->next;
	}

	ad.used -= freeNode->blockSize;
	coalescence(prev, freeNode);
}
using String = std::basic_string<char, std::char_traits<char>, Allocator<char>>;

template <typename T> using List = std::vector<T, Allocator<T>>;
using Bytes = List<char>;
} // namespace TemStream

namespace std
{
template <> struct hash<TemStream::String>
{
	std::size_t operator()(const TemStream::String &s) const
	{
		std::size_t value = s.size();
		for (auto c : s)
		{
			TemStream::hash_combine(value, c);
		}
		return value;
	}
};
} // namespace std