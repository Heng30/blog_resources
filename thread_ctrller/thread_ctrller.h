/* @desc:
 *  线程池模型，可以通过id将回调函数放入线程队列，也可以让程序选择适当的线程队列
 */
#ifndef _THREAD_CTRLLER_H_
#define _THREAD_CTRLLER_H_

#include <stdbool.h>

#define THREAD_CTRLLER_STATUS_SHARE 0X1 /* 线程共享，可以被随机分配 */
#define THREAD_CTRLLER_STATUS_UNSHARE 0X2

typedef void* (*thread_ctrller_cb_t) (void *arg);

/* @func:
 *  初始化
 * @ret:
 *  大于0：控制器节点数量
 *  -1: 出错
 */
int thread_ctrller_init(int thread_cnt);

/* @func:
 *  设置一个线程
 */
bool thread_ctrller_set(int id, size_t high_water, unsigned char status);

/* @func
 *  向一个线程添加任务
 * @param:
 *  id不在范围内，会选择一个深度最浅的共享线程添加任务
 */
bool thread_ctrller_add(int id, thread_ctrller_cb_t cb, void *arg);

/* @func:
 *  启动线程控制器
 */
bool thread_ctrller_dispatch(void);

/* @func:
 *  销毁线程控制器
 */
void thread_ctrller_destroy(void);

/* @func:
 *  打印一个节点
 */
void thread_ctrller_dump(int id);

/* @func:
 *  打印所有节点
 */
void thread_ctrller_dump_all(void);

#endif
