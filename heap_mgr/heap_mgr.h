#ifndef _HEAP_MGR_H_
#define _HEAP_MGR_H_

#include <stdbool.h>

typedef bool (*HEAP_MEMBER_COMPARE_T) (void *src, void *dst);
typedef void* (*HEAP_MGR_ALLOC_T) (size_t size);
typedef void (*HEAP_MGR_FREE_T) (void *ptr);

typedef struct _heap_mgr_member {
    void *m_ptr;
} heap_mgr_member_t;

typedef struct _heap_mgr {
    size_t m_capacity; /* 这个heap的大小 */
    size_t m_offset; /* 当前可以插入新节点的数组下标 */
    HEAP_MEMBER_COMPARE_T m_compare; /* 比较函数 */
    heap_mgr_member_t *m_heap;
    pthread_mutex_t m_mutex;
} heap_mgr_t;


/* @func:
 *  初始化参数
 */
void heap_mgr_init(HEAP_MGR_ALLOC_T alloc, HEAP_MGR_FREE_T dealloc);

/* @param:
 *  分配一个heap_mgr
 */
heap_mgr_t* heap_mgr_new(size_t capacity, HEAP_MEMBER_COMPARE_T compare);

/* @func:
 *  销毁heap_mgr，释放内存
 * @warn:
 *  非线程安全
 */
bool heap_mgr_free(heap_mgr_t *hm);

/* @func:
 *  添加一个节点到heap
 */
bool heap_mgr_member_add(heap_mgr_t *hm, void *ptr);

/* func:
 *  删除一个节点 */
void* heap_mgr_member_del(heap_mgr_t *hm);

/* @func:
 *  判断heap是否为空
 */
bool heap_mgr_is_empty(heap_mgr_t *hm);

#endif
