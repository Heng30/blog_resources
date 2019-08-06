#ifndef _CACHE_MGR_H_
#define _CACHE_MGR_H_

#include <stdbool.h>


typedef void* (*cache_mgr_alloc_t)(size_t size);
typedef void (*cache_mgr_free_t)(void *ptr);

typedef struct _cache_mgr {
    size_t m_size; /* 一个缓存的大小 */
    size_t m_cnt; /* 缓存的数量 */
    size_t m_index; /* 当前的缓存数量 */
    size_t m_ref; /* 引用的缓存数 */
    void *m_cache;
} cache_mgr_t;

/* @func
 *  初始化管理器
 * @param:
 *  cache_max: 全局数组的大小，管理器的容量
 */
bool cache_mgr_init(int cache_max, bool is_using_mutex, cache_mgr_alloc_t alloc, cache_mgr_free_t dealloc);

/* @func:
 *  销毁管理器
 */
void cache_mgr_destroy(void);

/* @func: 
 *  创建一个管理器
 * @param:
 *  size: 缓存区的大小
 *  cnt: 缓存区的上限
 */
int cache_mgr_new(size_t size, size_t cnt);

/* @func:
 *  销毁一个管理器
 */
void cache_mgr_free(int id);

/* func:
 *  获取一个缓存
 */
void* cache_mgr_get(int id);

/* @func:
 *  归还一个缓存
 * @warn:
 *  必须确保ptr是从cache_mgr_get中获取的缓存
 */
void cache_mgr_ret(int id, void *ptr);
#endif