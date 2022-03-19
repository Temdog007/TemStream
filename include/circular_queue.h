#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct CircularQueue
{
    uint8_t* data;
    size_t len;
    size_t start;
    size_t end;
} CQueue, *pCQueue;

extern CQueue CQueueCreate(size_t);

extern void CQueueFree(pCQueue);

extern size_t
CQueueCount(const CQueue*);

extern void
CQueueEnqueue(pCQueue, const uint8_t*, const size_t);

extern size_t
CQueueDequeue(pCQueue, uint8_t*, const size_t, const bool peek);

extern bool
CQueueCopy(pCQueue, const CQueue*, const Allocator*);