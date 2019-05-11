#ifndef _MEMORY_POOL_MGR_H_
#define _MEMORY_POOL_MGR_H_

#include <stdbool.h>

/* @func:
 *      通用内存分配器
 */
void* memory_pool_mgr_alloc(size_t size);
        
/* @func:
 *		通用内存销毁
 */
void memory_pool_mgr_free(void *ptr);

/* @func:
 *      打印所有统计信息
 */
void memory_pool_mgr_dump(void);


/* @func:
 *      初始化程序使用的内存池
 */
bool memory_pool_mgr_init(size_t start_size, size_t step_size, unsigned short member_size);

/* @func:
 *      销毁全部内存池，仅在程序退出时使用
 */
void memory_pool_mgr_destroy(void);

/* @func:
 *      清空所有缓冲池的自由buffer
 */
void memory_pool_mgr_gc(void);

#endif 
