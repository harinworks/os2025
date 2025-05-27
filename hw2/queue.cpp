#include <iostream>
#include <cstdlib>
#include "queue.h"

#if defined(CONFIG_MALLOC_ALIGNED)
// https://android.googlesource.com/platform/bionic/+/master/libc/include/sys/cdefs.h

#ifndef __BIONIC_ALIGN
#define __BIONIC_ALIGN(__value, __alignment) (((__value) + (__alignment) - 1) & ~((__alignment) - 1))
#endif

#if defined(__i386__)
#define PAGE_SIZE 4096
#define CACHE_SIZE 32
#elif defined(__x86_64__)
#define PAGE_SIZE 4096
#define CACHE_SIZE 64
#else
#define PAGE_SIZE 1
#define CACHE_SIZE 1
#endif

#ifdef _WIN32
inline void* internal_malloc(std::size_t size) {
	if (size >= PAGE_SIZE)
        return _aligned_malloc(__BIONIC_ALIGN(size, PAGE_SIZE), PAGE_SIZE);

	return _aligned_malloc(size, CACHE_SIZE);
}

inline void internal_free(void* ptr) {
	_aligned_free(ptr);
}
#else
#include <sys/mman.h>

inline void* internal_malloc(std::size_t size) {
	void* ptr;

	if (size >= PAGE_SIZE) {
        size = __BIONIC_ALIGN(size, PAGE_SIZE);

        if (posix_memalign(&ptr, PAGE_SIZE, size) == -1)
			return nullptr;

        madvise(ptr, PAGE_SIZE, MADV_WILLNEED);
        madvise(ptr, PAGE_SIZE, MADV_HUGEPAGE);
    } else if (posix_memalign(&ptr, CACHE_SIZE, size) == -1) {
        return nullptr;
    }

	return ptr;
}

inline void internal_free(void* ptr) {
	free(ptr);
}
#endif
#else
inline void* internal_malloc(std::size_t size) {
	return std::malloc(size);
}

inline void internal_free(void* ptr) {
	std::free(ptr);
}
#endif

#if defined(CONFIG_MUTEX_USE_STL)
#include <mutex>

inline void internal_lock(Queue* queue) {
	queue->mutex.lock();
}

inline void internal_unlock(Queue* queue) {
	queue->mutex.unlock();
}
#elif defined(CONFIG_MUTEX_USE_PTHREAD)
#include <pthread.h>

inline void internal_lock(Queue* queue) {
	pthread_mutex_lock(&queue->mutex);
}

inline void internal_unlock(Queue* queue) {
	pthread_mutex_unlock(&queue->mutex);
}
#elif defined(CONFIG_MUTEX_USE_SPINLOCK)
// https://wiki.osdev.org/Spinlock

#if defined(__i386__) || defined(__x86_64__)
inline void internal_lock(Queue* queue) {
	__asm__ __volatile__ (
        "1:\n"
        "   lock btsl $0, %0\n"
        "   jnc 3f\n"
        "2:\n"
        "   pause\n"
        "   testl $1, %0\n"
        "   jnz 2b\n"
		"   jmp 1b\n"
		"3:\n"
		:
        : "m"(queue->lock)
        : "memory", "cc"
    );
}

inline void internal_unlock(Queue* queue) {
	__asm__ __volatile__(
		"movl $0, %0\n"
		: "=m"(queue->lock)
		:
		: "memory"
	);
}
#else
inline void internal_lock(Queue* queue) {
	while (&queue->lock != 0);
	queue->lock = 1;
}

inline void internal_unlock(Queue* queue) {
	queue->lock = 0;
}
#endif
#else
inline void internal_lock(Queue* queue) {}
inline void internal_unlock(Queue* queue) {}
#endif

Queue* init(void) {
	return new Queue { .head = nullptr, .tail = nullptr };
}

void release(Queue* queue) {
	if (queue != nullptr)
		delete queue;
}

Node* nalloc(Item item) {
	auto node = reinterpret_cast<Node*>(internal_malloc(sizeof(Node)));
	*node = { .item = item, .next = nullptr };

	return node;
}

void nfree(Node* node) {
	if (node != nullptr)
		internal_free(node);
}

Node* nclone(Node* node) {
	if (node == nullptr)
		return nullptr;

	auto new_node = nalloc(node->item);

	if (new_node == nullptr)
		return nullptr;

	new_node->next = node->next;

	return new_node;
}

Reply enqueue(Queue* queue, Item item) {
	Reply reply = { false, item };

	if (queue == nullptr)
		return reply;

	internal_lock(queue);

	auto node = queue->tail;

	auto block_idx = node != nullptr && node->block_idx < CONFIG_BLOCK_LEN - 1
		? node->block_idx + 1
		: 0;

	auto block_root =
		block_idx > 0
		? node->block_root
		: internal_malloc(sizeof(Node) * CONFIG_BLOCK_LEN);

	if (block_root == nullptr) {
		internal_unlock(queue);
		return reply;
	}

	auto new_node = &reinterpret_cast<Node*>(block_root)[block_idx];
	*new_node = { .item = item, .next = nullptr, .block_root = block_root, .block_idx = block_idx };

	if (queue->head == nullptr) {
		queue->head = new_node;
		queue->tail = new_node;
	} else {
		node->next = new_node;
		queue->tail = new_node;
	}

	internal_unlock(queue);

	reply.success = true;

	return reply;
}

Reply dequeue(Queue* queue) {
	Reply reply = { false, { 0, nullptr } };

	if (queue == nullptr)
		return reply;

	internal_lock(queue);

	if (queue->head == nullptr) {
		internal_unlock(queue);
		return reply;
	}

	auto node = queue->head;

	reply.item = node->item;
	queue->head = node->next;

	if (queue->head == nullptr)
		queue->tail = nullptr;

	if (node->block_root != nullptr && (node->next == nullptr || node->next->block_root != node->block_root))
		internal_free(node->block_root);

	internal_unlock(queue);

	reply.success = true;

	return reply;
}

Queue* range(Queue* queue, Key start, Key end) {
	if (queue == nullptr)
		return nullptr;

	auto new_queue = init();

	if (new_queue == nullptr)
		return nullptr;

	internal_lock(queue);

	for (auto node = queue->head; node != nullptr; node = node->next) {
		if (node->item.key >= start && node->item.key <= end) {
			if (!enqueue(new_queue, node->item).success) {
				internal_unlock(queue);
				release(new_queue);
				return nullptr;
			}
		}
	}

	internal_unlock(queue);

	return new_queue;
}
