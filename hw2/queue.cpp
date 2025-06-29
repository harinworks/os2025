#include <iostream>
#include <cstdlib>
#include <cstring>
#include "qtype.h"
#include "queue.h"

#if defined(CONFIG_HACK)
#if defined(CONFIG_ENV_WIN32)
#pragma optimize("gt", on)
#pragma optimize("", on)
#pragma optimize("s", on)
#pragma optimize("y", on)
#pragma optimize("a", on)

#pragma strict_gs_check(off)
#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma vtordisp(off)

#pragma inline_recursion(on)
#pragma inline_depth(255)
#pragma auto_inline(on)

#pragma check_stack(off)
#pragma strict_gs_check(off)
#pragma detect_mismatch("","")
#pragma optimize("tg",on)

#include <Windows.h>
#include <Processthreadsapi.h>

static QUEUE_INLINE void internal_hack() {
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
}
#else
static QUEUE_INLINE void internal_hack() {}
#endif
#endif

#if defined(CONFIG_MALLOC_ALIGNED)
// https://android.googlesource.com/platform/bionic/+/master/libc/include/sys/cdefs.h

#ifndef __BIONIC_ALIGN
#define __BIONIC_ALIGN(__value, __alignment) (((__value) + (__alignment) - 1) & ~((__alignment) - 1))
#endif

#if defined(CONFIG_ENV_WIN32)
#include <intrin.h>

#define INTERNAL_PREFETCH(ptr, locality) _mm_prefetch(reinterpret_cast<const char*>(ptr), (locality))

static QUEUE_INLINE void* internal_malloc(std::size_t size) {
    if (size >= PAGE_SIZE)
        return _aligned_malloc(__BIONIC_ALIGN(size, PAGE_SIZE), PAGE_SIZE);

    return _aligned_malloc(size, CACHE_SIZE);
}

static QUEUE_INLINE void internal_free(void* ptr) {
    _aligned_free(ptr);
}
#else
#include <sys/mman.h>

#define INTERNAL_PREFETCH(ptr, locality) __builtin_prefetch((ptr), 0, (locality))

static QUEUE_INLINE void* internal_malloc(std::size_t size) {
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

static QUEUE_INLINE void internal_free(void* ptr) {
    std::free(ptr);
}
#endif
#else
static QUEUE_INLINE void* internal_malloc(std::size_t size) {
    return std::malloc(size);
}

static QUEUE_INLINE void internal_free(void* ptr) {
    std::free(ptr);
}
#endif

#if defined(CONFIG_MUTEX_USE_STL)
#include <mutex>

static QUEUE_INLINE void internal_lock(Queue* queue) {
    queue->mutex.lock();
}

static QUEUE_INLINE void internal_unlock(Queue* queue) {
    queue->mutex.unlock();
}
#elif defined(CONFIG_MUTEX_USE_PTHREAD)
#include <pthread.h>

static QUEUE_INLINE void internal_lock(Queue* queue) {
    pthread_mutex_lock(&queue->mutex);
}

static QUEUE_INLINE void internal_unlock(Queue* queue) {
    pthread_mutex_unlock(&queue->mutex);
}
#elif defined(CONFIG_MUTEX_USE_WINAPI)
#include <Windows.h>

static QUEUE_INLINE void internal_lock(Queue* queue) {
    EnterCriticalSection(&queue->mutex);
}

static QUEUE_INLINE void internal_unlock(Queue* queue) {
    LeaveCriticalSection(&queue->mutex);
}
#elif defined(CONFIG_MUTEX_USE_SPINLOCK)
// https://wiki.osdev.org/Spinlock

#if defined(CONFIG_ENV_WIN32)
// MSVC's inline ASM statement is not as good as GCC/Clang, so it might not work for Windows...
// It will cause compile or memory errors...

static QUEUE_INLINE void internal_lock(Queue* queue) {
    volatile auto lock_ptr = queue->lock;

    __asm {
    acquire:
        lock bts dword ptr [lock_ptr], 0
        jnc exit
    spin:
        pause
        test dword ptr [lock_ptr], 1
        jnz spin
        jmp acquire
    exit:
    }
}

static QUEUE_INLINE void internal_unlock(Queue* queue) {
    volatile auto lock_ptr = queue->lock;

    __asm {
        mov dword ptr [lock_ptr], 0
    }
}
#else
static QUEUE_INLINE void internal_lock(Queue* queue) {
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

static QUEUE_INLINE void internal_unlock(Queue* queue) {
    __asm__ __volatile__(
        "movl $0, %0\n"
        : "=m"(queue->lock)
        :
        : "memory"
    );
}
#endif
#else
static QUEUE_INLINE void internal_lock(Queue* queue) {}
static QUEUE_INLINE void internal_unlock(Queue* queue) {}
#endif

static Node** find_tree_node(Queue* queue, const Item& item, bool is_overwrite = false) {
    auto node_ptr = &queue->tree_root;

    while (*node_ptr != nullptr) {
        auto node = *node_ptr;
        auto& node_item = node->item;

        if (node->tree_left != nullptr)
            INTERNAL_PREFETCH(node->tree_left, 1);
        if (node->tree_right != nullptr)
            INTERNAL_PREFETCH(node->tree_right, 1);

        if (item.key < node_item.key) {
            node_ptr = &node->tree_left;
        } else if (item.key > node_item.key) {
            node_ptr = &node->tree_right;
        } else if (is_overwrite) {
            internal_free(node_item.value);

            // New memory was already ready, do not deep copy on here
            node_item.value = item.value;
            node_item.value_size = item.value_size;

            return nullptr;
        } else {
            break;
        }
    }

    return node_ptr;
}

Queue* init(void) {
#if defined(CONFIG_HACK)
    static auto is_inited = false;

    if (!is_inited) {
        internal_hack();
        is_inited = true;
    }
#endif

    auto queue = reinterpret_cast<Queue*>(internal_malloc(sizeof(Queue)));

    if (queue == nullptr)
        return nullptr;

    new (queue) Queue { nullptr, nullptr, nullptr };

#if defined(CONFIG_MUTEX_USE_WINAPI)
    InitializeCriticalSection(&queue->mutex);
#endif

    return queue;
}

void release(Queue* queue) {
    if (queue == nullptr)
        return;

#if defined(CONFIG_MUTEX_USE_WINAPI)
        DeleteCriticalSection(&queue->mutex);
#endif

    queue->~Queue();
    internal_free(queue);
}

Node* nalloc(Item item) {
    auto node = reinterpret_cast<Node*>(internal_malloc(sizeof(Node)));

    if (node == nullptr)
        return nullptr;

    *node = { item, nullptr, nullptr, 0 };

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

    item.value = internal_malloc(item.value_size);    // Only for internal use

    if (item.value != nullptr && reply.item.value != nullptr)
        std::memcpy(item.value, reply.item.value, item.value_size);

    internal_lock(queue);

    auto tree_node_ptr = find_tree_node(queue, item, true);

    if (tree_node_ptr == nullptr) {
        // Existing node item has been overwritten
        internal_unlock(queue);
        reply.success = true;
        return reply;
    }

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
        internal_free(item.value);
        return reply;
    }

    auto new_node = &reinterpret_cast<Node*>(block_root)[block_idx];
    *new_node = { item, nullptr, nullptr, nullptr, block_root, block_idx };

    if (queue->head == nullptr) {
        queue->head = new_node;
        queue->tail = new_node;
    } else {
        node->next = new_node;
        queue->tail = new_node;
    }

    (*tree_node_ptr) = new_node;

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

    reply.item.key = node->item.key;
    reply.item.value_size = node->item.value_size;
    reply.item.value = std::malloc(reply.item.value_size);    // Not for internal use
    
    if (reply.item.value != nullptr && node->item.value != nullptr)
        std::memcpy(reply.item.value, node->item.value, reply.item.value_size);

    queue->head = node->next;

    auto tree_node_ptr = find_tree_node(queue, node->item);

    // Prune tree nodes
    if (*tree_node_ptr != nullptr) {
        auto left_node = (*tree_node_ptr)->tree_left;
        auto right_node = (*tree_node_ptr)->tree_right;

        if (left_node != nullptr) {
            *tree_node_ptr = left_node;
            left_node->tree_right = right_node;
        } else if (right_node != nullptr) {
            *tree_node_ptr = right_node;
            right_node->tree_left = left_node;
        } else {
            *tree_node_ptr = nullptr;
        }
    }

    if (queue->head == nullptr)
        queue->tail = nullptr;

    internal_free(node->item.value);

    if (node->next != nullptr)
        INTERNAL_PREFETCH(node->next, 2);

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
        if (node->next != nullptr)
            INTERNAL_PREFETCH(node->next, 2);

        auto key = node->item.key;

        if (key >= start && key <= end) {
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
