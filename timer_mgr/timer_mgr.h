#ifndef _TIMER_MGR_H_
#define _TIMER_MGR_H_
#include <stdbool.h>

typedef void* (*timer_mgr_cb_t)(void *arg);
typedef void* (*timer_mgr_alloc_t) (size_t size);
typedef void (*timer_mgr_free_t) (void *ptr);

/* @desc:
 *	用于执行时间不长的定时任务，
 *	执行时间过长的定时任务会造成后面的定时任务延时较长时间,
 *	定时任务并不会精确的定时执行，时间粒度是很大的
 */
typedef struct _timer_mgr {
	timer_mgr_cb_t m_cb; /* 回调函数 */ 
	void *m_arg; /* 回调函数的参数 */
	int m_interval; /* 时间间隔 */
	int m_remain; /* 剩余时间 */
} timer_mgr_t;

/* @func:
 *	初始化管理器
 */
bool timer_mgr_init(size_t timer_max, timer_mgr_alloc_t alloc, timer_mgr_free_t dealloc);

/* @func:
 *	销毁管理器
 */
void timer_mgr_destroy(void);

/* @func:
 *	创建一个定时器
 */
int timer_mgr_new(timer_mgr_cb_t cb, void* arg, int interval);

/* @func:
 *	通过id释放一个定时器
 */
void timer_mgr_free_by_id(size_t id);

/* @func:
 *	通过一个回调函数释放一个定时器
 */
void timer_mgr_free_by_cb(timer_mgr_cb_t cb);

/* @func:
 *	停止定时管理器，释放资源
 */
void timer_mgr_stop(void);

/* @func:
 *	定时调用回调函数
 */
void timer_mgr_dispatch(void);

#endif
