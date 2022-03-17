#include <include/main.h>

CQueue
CQueueCreate(const size_t len)
{
    uint8_t* data = currentAllocator->allocate(len);
    return (CQueue){ .data = data, .len = len, .start = 0, .end = 0 };
}

void
CQueueFree(pCQueue queue)
{
    currentAllocator->free(queue->data);
}

size_t
CQueueCount(const CQueue* queue)
{
    return queue->end - queue->start;
}

void
CQueueEnqueue(pCQueue queue, const uint8_t* data, size_t len)
{
    const size_t endPtr = queue->end % queue->len;
    const size_t left = queue->len - endPtr;
    if (left < len) {
        memcpy(queue->data + endPtr, data, left);
        memcpy(queue->data, data + left, len - left);
    } else {
        memcpy(queue->data + endPtr, data, len);
    }
    queue->end += len;
}

size_t
CQueueDequeue(pCQueue queue, uint8_t* data, const size_t len, const bool peek)
{
    const size_t startPtr = queue->start % queue->len;
    const size_t endPtr = queue->end % queue->len;
    const size_t total = CQueueCount(queue);
    const size_t toCopy = SDL_min(len, total);
    if (endPtr < startPtr) {
        const size_t left = queue->len - startPtr;
        if (left < toCopy) {
            memcpy(data, queue->data + startPtr, left);
            memcpy(data + left, queue->data, toCopy - left);
        } else {
            memcpy(data, queue->data + startPtr, toCopy);
        }
    } else {
        memcpy(data, queue->data + startPtr, toCopy);
    }
    if (!peek) {
        queue->start += toCopy;
    }
    return toCopy;
}