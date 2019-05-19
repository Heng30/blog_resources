#ifndef _GLOBAL_MGR_H_
#define _GLOBAL_MGR_H_

typedef void* (*global_mgr_alloc_t)(size_t size);
typedef void (*global_mgr_free_t) (void *ptr);

typedef struct {} global_mgr_member_t;

/* @func:
 *	初始化管理器
 */
bool global_mgr_init(size_t size, global_mgr_alloc_t global_alloc, global_mgr_free_t global_free,
								global_mgr_alloc_t member_alloc, global_mgr_free_t member_free);

/* @func:
 *	销毁管理器，仅用在程序退出时使用
 */
void global_mgr_destroy(void);

/* @func:
 *	分配一个管理节点的成员
 */
bool global_mgr_member_new(size_t index);

/* @func:
 *	释放管理器成员占用的空间
 */
bool global_mgr_member_free(size_t index);

/* @func:
 *	获取一个管理器的成员
 */
global_mgr_member_t* global_mgr_member_get(size_t index);

/* @func:
 *	返还管理器的成员
 */
bool global_mgr_member_ret(size_t index);

void global_mgr_dump(size_t index);

void global_mgr_dump_all(void);

#endif