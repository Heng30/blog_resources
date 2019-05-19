#ifndef _THREAD_POOL_MGR_H_
#define _THREAD_POOL_MGR_H_

#include <stdbool.h>

typedef void* (*thread_pool_mgr_alloc_t) (size_t size);
typedef void (*thread_pool_mgr_free_t) (void *ptr);
typedef void* (*thread_pool_mgr_task_func_t)(void *arg);

/* @func:
 *	初始化线程池
 * @warn:
 *  alloc函数必须保证初始化内存的值为0
 */
bool thread_pool_mgr_init(size_t thread_size, size_t task_size,
					thread_pool_mgr_alloc_t alloc, thread_pool_mgr_free_t dealloc);
/* @func:
 *	 销毁线程池占用的内存
 */
bool thread_pool_mgr_destroy(void);

/* @func:
 *  等待所有线程返回
 */
bool thread_pool_mgr_dispatch(void);


/* @func:
 *	 销毁线程池占用的内存
 */
bool thread_pool_mgr_destroy(void);

/* func:
 *	添加任务
 */
bool thread_pool_mgr_task_add(void* (*func)(void *arg), void *arg);

/* @func:
 *  打印信息
 */
void thread_pool_mgr_dump(void);

#endif
