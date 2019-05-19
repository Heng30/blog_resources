#ifndef _QUEUE_MGR_H_
#define _QUEUE_MGR_H_
#include <stdbool.h>

typedef void* (*queue_mgr_alloc_t) (size_t size);
typedef void (*queue_mgr_free_t) (void *ptr);
typedef void (*queue_mgr_ptr_free_t) (void *ptr); /* member的ptr成员的释放内存函数 */

typedef struct _queue_mgr_member {
	void* m_ptr; /* 用于保存指针 */
    struct _queue_mgr_member *m_prev;
    struct _queue_mgr_member *m_next;
} queue_mgr_member_t;

typedef struct _queue_mgr {
	queue_mgr_member_t *m_member;
    pthread_mutex_t m_mutex;
} queue_mgr_t;

/* @func:
 *  初始化队列
 */
void queue_mgr_init(queue_mgr_alloc_t alloc, queue_mgr_free_t decalloc);

/* @func:
 * 创建一个队列
 */
queue_mgr_t* queue_mgr_new(void);

/* @func:
 *  销毁一个队列
 */

bool queue_mgr_free(queue_mgr_t *qm, queue_mgr_ptr_free_t dealloc);

/* @func:
 *  入队列头
 */
bool queue_mgr_push_front(queue_mgr_t *qm, void* ptr);

/* @func:
 *  入队列尾
 */
bool queue_mgr_push_back(queue_mgr_t *qm, void* ptr);

/* @func:
 * 出队列头
 */
void* queue_mgr_pop_front(queue_mgr_t *qm);

/* @func:
 * 出队列尾
 */
void* queue_mgr_pop_back(queue_mgr_t *qm);

/* @func:
 *  是否为空
 */
bool queue_mgr_is_empty(queue_mgr_t *qm);

/* @func:
 *  打印信息
 */
void queue_mgr_dump(queue_mgr_t *qm);

#endif
