#ifndef _QTYPE_H  // header guard
#define _QTYPE_H

// ==========이 파일은 수정 가능==========

#include <cstddef>

#define CONFIG_BLOCK_LEN 64

#define CONFIG_MALLOC_ALIGNED y
#define CONFIG_MALLOC_ALIGNED_SIZE 16

// #define CONFIG_MUTEX_USE_STL y
// #define CONFIG_MUTEX_USE_PTHREAD y
#define CONFIG_MUTEX_USE_SPINLOCK y

#if defined(CONFIG_MUTEX_USE_STL)
#include <mutex>
#elif defined(CONFIG_MUTEX_USE_PTHREAD)
#include <pthread.h>
#endif

typedef unsigned int Key;  // 값이 클수록 높은 우선순위
typedef void* Value;

typedef struct {
    Key key;
    Value value;
} Item;

typedef struct {
    bool success;   // true: 성공, false: 실패
    Item item;
    // 필드 추가 가능
} Reply;

typedef struct node_t {
    Item item;
    struct node_t* next;
    // 필드 추가 가능
    void *block_root;
    std::size_t block_idx;
} Node;

typedef struct {
    Node* head, * tail;
    // 필드 추가 가능
#if defined(CONFIG_MUTEX_USE_STL)
    std::mutex mutex;
#elif defined(CONFIG_MUTEX_USE_PTHREAD)
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#elif defined(CONFIG_MUTEX_USE_SPINLOCK)
    volatile void* lock = nullptr;
#endif
} Queue;

// 이후 자유롭게 추가/수정: 새로운 자료형 정의 등

#endif
