/* @desc:
 *  1. 使用ucontext.h头文件相关的函数实现用户级线程（协程)
 *  2. 注意：
 *      1. 任务的数量在一开始设定，不会自动扩容，任务的数量一般都比较好确定，不自动扩容问题也不大
 *      2. 任务不应该在不同的线程之间共享
 *      3. 所有的任务都应该正常退出，否则会造成无限循环
 */

#ifndef _UTHREAD_MGR_H_
#define _UTHREAD_MGR_H_

typedef struct _uthread_mgr uthread_mgr_t;
typedef struct _uthread_task uthread_task_t;

typedef void* (*uthread_mgr_alloc_t) (size_t size);
typedef void (*uthread_mgr_free_t) (void *ptr);
typedef void* (*uthread_mgr_func_t) (uthread_mgr_t *um, void *arg);

struct _uthread_mgr {
    int m_task_count; /* 能够容纳的任务数 */
    int m_task_occupy; /* 总共添加的任务数 */
    int m_finish_hint; /* 每一次suspend加1，yield减1，等于task_occupy时则表示任务结束 */
    int m_cur_task_index;  /* 当前正在运行的任务下标 */
    uthread_task_t *m_task;
};

/* @func
 *  初始化管理器
 */
void uthread_mgr_init(uthread_mgr_alloc_t alloc, uthread_mgr_free_t dealloc);

/* @func:
 *  创建一个管理器
 */
uthread_mgr_t* uthread_mgr_new(int task_count);

/* @func:
 *  销毁管理器
 */
void uthread_mgr_free(uthread_mgr_t *um);

/* @func:
 *  添加一个任务
 */
int uthread_mgr_add(uthread_mgr_t *um, uthread_mgr_func_t func, void *arg);

/* @func:
 *  切换到主任务
 */
void uthread_mgr_yeild(uthread_mgr_t *um);

/* @func:
 *  重新回到断点开始执行
 */
void uthread_mgr_resume(uthread_mgr_t *um, int id);

/* @func:
 *  判断所有的任务是否完成
 */
bool uthread_mgr_finish(uthread_mgr_t *um);

/* @func:
 *  打印信息
 */
void uthread_mgr_dump(uthread_mgr_t *um, bool is_dump_task);

#endif
