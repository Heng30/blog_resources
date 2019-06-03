/* @desc:
 *	用途：
 *		1. 在多线程的环境里，控制同一个函数仅能在一个线程中运行，其他线程只能等待该函数执行完才能再次执行该函数；
 *		2. 一个函数只允许执行N次
 *	注意：
 *		销毁管理器是非线程安全的
 */

#ifndef _SYNC_N_MGR_H_
#define _SYNC_N_MGR_H_

typedef void* (*sync_n_mgr_alloc_t) (size_t size);
typedef void (*sync_n_mgr_free_t) (void *ptr);
typedef void* (*sync_n_mgr_func_t) (void *arg);

typedef struct _sync_n_mgr {
	size_t m_run_count; // 指定的运行次数
	sync_n_mgr_func_t m_func; // 运行的函数
	void *m_arg; // func的参数
	pthread_mutex_t m_mutex;
}sync_n_mgr_t;

/* @func:
 *	初始化管理器
 */
void sync_n_mgr_init(sync_n_mgr_alloc_t alloc, sync_n_mgr_free_t dealloc);

/* @func:
 *	创建管理器
 */
sync_n_mgr_t* sync_n_mgr_new(size_t run_count, sync_n_mgr_func_t func, void *arg);

/* @func:
 *	销毁管理器
 */
void sync_n_mgr_free(sync_n_mgr_t *sm);

/* @func:
 *	执行管理器关联的函数
 */
void* sync_n_mgr_do(sync_n_mgr_t *sm);

/* @func:
 *	打印管理器信息
 */
void sync_n_mgr_dump(sync_n_mgr_t *sm);

#endif
