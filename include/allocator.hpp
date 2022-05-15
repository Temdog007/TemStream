#pragma once

#include <main.hpp>

namespace TemStream
{
/**
 * @brief Placement policy for free list allocator
 */
enum class PlacementPolicy
{
	First, ///< Find first block that is big enough the handle the allocation request. The faster policy.
	Best
	///< Find smallest block that is big enough the handle the allocation request. Reduces chance of fragmentation
	///< in heap. The slower policy
};
/**
 * @brief Linked list used by free list allocator
 */
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

/**
 * @brief Data passed to all free list allocators
 */
class AllocatorData
{
  private:
	Mutex mutex;
	FreeListNode *list;
	void *data;
	size_t used;
	size_t len;
	size_t allocationNum;
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

	/**
	 * @brief Combine memory blocks if possible
	 *
	 * @param previousNode Node that came before freeNode
	 * @param freeNode Node that was just re-inserted into memory blcok
	 */
	void coalescence(FreeListNode *previousNode, FreeListNode *freeNode)
	{
		if (freeNode->next != nullptr &&
			reinterpret_cast<size_t>(freeNode) + freeNode->blockSize == reinterpret_cast<size_t>(freeNode->next))
		{
			freeNode->blockSize += freeNode->next->blockSize;
			FreeListNode::remove(list, freeNode, freeNode->next);
		}
		if (previousNode != nullptr &&
			reinterpret_cast<size_t>(previousNode) + previousNode->blockSize == reinterpret_cast<size_t>(freeNode))
		{
			previousNode->blockSize += freeNode->blockSize;
			FreeListNode::remove(list, previousNode, freeNode);
		}
	}

	/**
	 * @brief Find a valid memory block
	 *
	 * @param size Requested memory block size
	 * @param previousNode [out] the node before the foundNode
	 * @param foundNode [out] the node that contains the request memory block
	 */
	void find(const size_t size, FreeListNode *&previousNode, FreeListNode *&foundNode)
	{
		switch (policy)
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

	/**
	 * @brief See #TemStream::PlacementPolicy::First
	 *
	 * @param size Requested memory block size
	 * @param previousNode [out] the node before the foundNode
	 * @param foundNode [out] the node that contains the request memory block
	 */
	void findFirst(const size_t size, FreeListNode *&previousNode, FreeListNode *&foundNode)
	{
		FreeListNode *it = list;
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

	/**
	 * @brief See #TemStream::PlacementPolicy::Best
	 *
	 * @param size Requested memory block size
	 * @param previousNode [out] the node before the foundNode
	 * @param foundNode [out] the node that contains the request memory block
	 */
	void findBest(const size_t size, FreeListNode *&previousNode, FreeListNode *&foundNode)
	{
		size_t smallestDiff = SIZE_MAX;
		FreeListNode *bestBlock = nullptr;
		FreeListNode *bestPrevBlock = nullptr;
		FreeListNode *it = list;
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
	AllocatorData()
		: mutex(), list(nullptr), data(nullptr), used(0), len(0), allocationNum(0), policy(PlacementPolicy::Best)
	{
	}
	AllocatorData(const AllocatorData &) = delete;
	AllocatorData(AllocatorData &&) = delete;

	~AllocatorData()
	{
		close();
	}

	/**
	 * @brief Get total availble memory
	 *
	 * @return total available memory in bytes
	 */
	size_t getTotal() const
	{
		return len;
	}

	/**
	 * @brief Get amount of memory currently in use
	 *
	 * @return memory in use in bytes
	 */
	size_t getUsed() const
	{
		return used;
	}

	/**
	 * @brief Get number of allocation calls
	 *
	 * @return Number of allocation calls
	 */
	size_t getNum() const
	{
		return allocationNum;
	}

	/**
	 * Reset and re-allocate data. Does NOT re-assign pointers that were using the old memory block. Only use at startup
	 *
	 * @param len Amount of total memory to use
	 * @param policy
	 */
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

/**
 * Data used by default constructor of Allocators
 *
 * @ref TemStream::Allocator
 */
extern AllocatorData globalAllocatorData;

/**
 * @brief Free list allocator
 *
 * @tparam T type to allocate
 */
template <class T> class Allocator
{
  public:
	typedef T value_type;

	template <class U> friend class Allocator;

  private:
	AllocatorData &ad;

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

	/**
	 * @brief Allocate a number of type T
	 *
	 * @param n Number of T's to allocate
	 *
	 * @return pointer to allocated data
	 */
	T *allocate(const size_t n = 1);

	/**
	 * @brief Re-allocate a number of type T.
	 *
	 * Try to extend the current block. If not possible, allocate new block, copy old block data to new block data, and
	 * then free old block.
	 *
	 * @param ptr Pointer to the old data
	 * @param n Number of T's to allocate
	 *
	 * @return pointer to allocated data
	 */
	T *reallocate(T *ptr, const size_t n);

	/**
	 * @brief De-allocate the pointer
	 *
	 * @param p The pointer to free
	 * @param count unused
	 */
	void deallocate(T *const p, const size_t count = 1);

	/**
	 * @brief Call constructor on pointer with arguments
	 *
	 * @tparam Args the arguments to pass to the constructor
	 * @param t The pointer
	 * @param args the argumetns
	 */
	template <typename... Args> static void construct(T *t, Args &&...args)
	{
		new (t) T(std::forward<Args>(args)...);
	}

	/**
	 * @brief Call destructor on pointer
	 *
	 * @param p the pointer
	 */
	static void destroy(T *p)
	{
		p->~T();
	}

	/**
	 * @brief Get size of block from pointer
	 *
	 * @param p the poitner
	 *
	 * @return The size of the block
	 */
	size_t getBlockSize(const T *const p) const;
};

#if __ANDROID__
#define ALLOCATOR_ALIGNMENT 16
#else
#define ALLOCATOR_ALIGNMENT (2 * sizeof(void *))
#endif

template <class T> T *Allocator<T>::allocate(const size_t requestedCount)
{
	// STL containers will call allocate with size 0. So, nullptr is valid
	if (requestedCount == 0)
	{
		return nullptr;
	}

	const size_t requestedSize = sizeof(T) * requestedCount;

	LOCK(ad.mutex);

	// Align memory just to be safe
	size_t size = std::max<size_t>(requestedSize, ALLOCATOR_ALIGNMENT);
	size += ALLOCATOR_ALIGNMENT - (size % ALLOCATOR_ALIGNMENT);
	const size_t allocateSize = size + sizeof(FreeListNode);

	FreeListNode *affectedNode = nullptr;
	FreeListNode *previousNode = nullptr;
	ad.find(allocateSize, previousNode, affectedNode);

	// If null, then there is no block that can handle the requestedSize
	if (affectedNode == nullptr)
	{
		throw std::bad_alloc();
	}

	const size_t rest = affectedNode->blockSize - allocateSize;

	// If block has extra size, split the block into 2 and insert the remaining chunk back
	// into the linked list
	if (rest > 0)
	{
		FreeListNode *newFreeNode =
			reinterpret_cast<FreeListNode *>(reinterpret_cast<size_t>(affectedNode) + allocateSize);
		newFreeNode->blockSize = rest;
		newFreeNode->next = nullptr;
		FreeListNode::insert(ad.list, affectedNode, newFreeNode);
	}

	// Remove the allocated data from the linked list.
	FreeListNode::remove(ad.list, previousNode, affectedNode);
	affectedNode->blockSize = allocateSize;
	affectedNode->next = nullptr;

	ad.used += allocateSize;

	const size_t dataAddress = reinterpret_cast<size_t>(affectedNode) + sizeof(FreeListNode);
	T *ptr = reinterpret_cast<T *>(dataAddress);
	++ad.allocationNum;
	return ptr;
}
template <class T> T *Allocator<T>::reallocate(T *oldPtr, const size_t count)
{
	LOCK(ad.mutex);

	if (oldPtr == nullptr)
	{
		return allocate(count);
	}

	// Align memory just to be safe
	size_t size = sizeof(T) * count;
	size = std::max<size_t>(size, ALLOCATOR_ALIGNMENT);
	size += ALLOCATOR_ALIGNMENT - (size % ALLOCATOR_ALIGNMENT);

	// Get the current memory block
	const size_t currentAddress = (size_t)oldPtr;
	const size_t nodeAddress = currentAddress - sizeof(FreeListNode);

	FreeListNode *node = reinterpret_cast<FreeListNode *>(nodeAddress);

	const size_t oldSize = node->blockSize - sizeof(FreeListNode);

	// Don't reduce the size of the current block. Just return.
	if (size <= oldSize)
	{
		return oldPtr;
	}

	{
		// Find the block that would be right after the current block. That is the only block that can be used to
		// extending the current block. Also, find the block before it in the linked list
		const size_t target = nodeAddress + node->blockSize;
		FreeListNode *it = ad.list;
		FreeListNode *prev = NULL;
		while (it != NULL)
		{
			if (reinterpret_cast<size_t>(it) != target)
			{
				prev = it;
				it = it->next;
				continue;
			}

			// The size of the current block and the block after it if they were combined
			const size_t combinedSize = node->blockSize + it->blockSize;

			// The size of the re-allocated block
			const size_t newBlockSize = size + sizeof(FreeListNode);

			// If the size of the two blocks is exactly the requested size, then just remove the block
			if (combinedSize == newBlockSize)
			{
				ad.used -= node->blockSize;
				ad.used += newBlockSize;
				node->blockSize = newBlockSize;
				FreeListNode::remove(ad.list, prev, it);
				return oldPtr;
			}

			// If the combined size is greater than the requested size, the block will need to be split. Then, the
			// remaining chunk can be inserted back into the list.
			else if (newBlockSize < combinedSize)
			{
				ad.used -= node->blockSize;
				ad.used += newBlockSize;
				node->blockSize = newBlockSize;
				FreeListNode *newNode = reinterpret_cast<FreeListNode *>(reinterpret_cast<size_t>(node) + newBlockSize);
				newNode->blockSize = combinedSize - newBlockSize;
				newNode->next = nullptr;
				FreeListNode::remove(ad.list, prev, it);
				FreeListNode::insert(ad.list, prev, newNode);
				return oldPtr;
			}

			// Not possible to re-allocate. Exit loop
			break;
		}
	}

	// At this point, it is determined that re-allocating is not possible.
	// So, allocate new block, copy old block to new block, and free old block
	T *newPtr = allocate(count);
	memcpy(newPtr, oldPtr, oldSize);
	deallocate(oldPtr);
	return newPtr;
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

	// Insert the block back into the list at the right spot
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

	// Combine adjacent blocks into one
	ad.coalescence(prev, freeNode);
	--ad.allocationNum;
}
template <class T> size_t Allocator<T>::getBlockSize(const T *const ptr) const
{
	if (ptr == nullptr)
	{
		return 0;
	}
	LOCK(ad.mutex);

	const size_t currentAddress = reinterpret_cast<size_t>(ptr);
	const size_t headerAddress = currentAddress - sizeof(FreeListNode);
	const FreeListNode *freeNode = reinterpret_cast<const FreeListNode *>(headerAddress);
	return freeNode->blockSize;
}
} // namespace TemStream

#include "allocator_defs.hpp"