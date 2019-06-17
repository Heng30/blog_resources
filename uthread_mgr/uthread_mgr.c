#include <stdio.h>
#include <ucontext.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "uthread_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define UTHREAD_MGR_TRACE_LOG MY_PRINTF
#define UTHREAD_MGR_DEBUG_LOG MY_PRINTF
#define UTHREAD_MGR_INFO_LOG MY_PRINTF
#define UTHREAD_MGR_WARN_LOG MY_PRINTF
#define UTHREAD_MGR_ERROR_LOG MY_PRINTF

#define UTHREAD_MGR_STACK_SIZE (128 * 1024)

#define UTHREAD_TASK_STATUS_INIT 0X0 /* 需要转为running状态 */
#define UTHREAD_TASK_STATUS_RUNNING 0X1 /* 需要转为suspend状态 */
#define UTHREAD_TASK_STATUS_SUSPEND 0X2 /* 需要转为running状态 */

struct _uthread_task {
    ucontext_t m_main_ctx; /* 主上下文 */
    ucontext_t m_ctx; /* 当前的上下文 */
    int m_main_index; /* 主上下文的下标 */
    uthread_mgr_func_t m_func;
    void *m_arg;
    unsigned char m_status;
    char m_stack[UTHREAD_MGR_STACK_SIZE];
};

static uthread_mgr_alloc_t g_um_alloc = NULL;
static uthread_mgr_free_t g_um_free = NULL;

static void* _malloc2calloc(size_t size)
{
    return calloc(1, size);
}

/* @func
 *  初始化管理器
 */
void uthread_mgr_init(uthread_mgr_alloc_t alloc, uthread_mgr_free_t dealloc)
{
    if (!alloc || !dealloc) g_um_alloc = _malloc2calloc, g_um_free = free;
    else g_um_alloc = alloc, g_um_free = dealloc;
}

/* @func:
 *  创建一个管理器
 */
uthread_mgr_t* uthread_mgr_new(int task_count)
{
    if (task_count <= 0) return NULL;
    uthread_mgr_t *um = NULL;
    int i = 0;
    size_t size = sizeof(uthread_mgr_t) + task_count * sizeof(uthread_task_t);


    if (!(um = (uthread_mgr_t*)g_um_alloc(size))) {
        UTHREAD_MGR_ERROR_LOG("g_um_alloc error, errno: %d - %s", errno, strerror(errno));
        return NULL;
    }

    um->m_task_count = task_count;
    um->m_finish_hint = 0;
    um->m_task = (uthread_task_t*)(um + 1);
    um->m_cur_task_index = -1;
    for (i = 0; i < task_count; i++) um->m_task[i].m_main_index = -1;
    return um;
}

/* @func:
 *  销毁管理器
 */
void uthread_mgr_free(uthread_mgr_t *um)
{
    if (!um) return ;
    g_um_free(um);
}

/* @func:
 *  添加一个任务
 */
int uthread_mgr_add(uthread_mgr_t *um, uthread_mgr_func_t func, void *arg)
{
    if (!um || !func) return -1;
    if (um->m_task_count <= um->m_task_occupy) return -1;
    int index = 0;

    index = um->m_task_occupy++;
    um->m_task[index].m_func = func;
    um->m_task[index].m_arg = arg;
    um->m_task[index].m_status = UTHREAD_TASK_STATUS_INIT;

    return index;
}

/* @func:
 *  切换到主任务
 */
void uthread_mgr_yeild(uthread_mgr_t *um)
{
    if (!um) return ;
    uthread_task_t *ut = NULL;
    if (um->m_cur_task_index == -1) return ;

    ut = &um->m_task[um->m_cur_task_index];
    ut->m_status = UTHREAD_TASK_STATUS_SUSPEND;
    um->m_finish_hint--;
    if (-1 == swapcontext(&ut->m_ctx, &ut->m_main_ctx)) {
        UTHREAD_MGR_WARN_LOG("swapcontext error, errno: %d - %s", errno, strerror(errno));
    }
}

/* @func:
 *  重新回到断点开始执行
 */
void uthread_mgr_resume(uthread_mgr_t *um, int id)
{
    if(!um || id < 0 || id >= um->m_task_occupy) return ;
    uthread_task_t *ut = &um->m_task[id];

    switch(ut->m_status){
        case UTHREAD_TASK_STATUS_INIT:
            if (-1 == getcontext(&ut->m_ctx)) {
                UTHREAD_MGR_WARN_LOG("getcontext error, errno: %d - %s", errno, strerror(errno));
                return ;
            }
            ut->m_ctx.uc_stack.ss_sp = ut->m_stack;
            ut->m_ctx.uc_stack.ss_size = sizeof(ut->m_stack);
            ut->m_ctx.uc_stack.ss_flags = 0;
            ut->m_ctx.uc_link = &ut->m_main_ctx;
            makecontext(&ut->m_ctx, (void (*)(void))ut->m_func, 2, um, ut->m_arg);
            /* fall throught */
        case UTHREAD_TASK_STATUS_SUSPEND:
            ut->m_status = UTHREAD_TASK_STATUS_RUNNING;
            ut->m_main_index = um->m_cur_task_index;
            um->m_cur_task_index = id;
            um->m_finish_hint++;
            if (-1 == swapcontext(&ut->m_main_ctx, &ut->m_ctx)) {
                UTHREAD_MGR_WARN_LOG("swapcontext error, errno: %d - %s", errno, strerror(errno));
            }
            um->m_cur_task_index = ut->m_main_index;

            break;
        default: ;
    }
}

/* @func:
 *  判断所有的任务是否完成
 */
bool uthread_mgr_finish(uthread_mgr_t *um)
{
    if (!um) return true;
    if (um->m_finish_hint == um->m_task_occupy) return true;
    return false;
}

/* @func:
 *  打印信息
 */
void uthread_mgr_dump(uthread_mgr_t *um, bool is_dump_task)
{
    if (!um) return ;
    int i = 0;
    uthread_task_t *ut = NULL;

    UTHREAD_MGR_TRACE_LOG("=======");
    UTHREAD_MGR_TRACE_LOG("task_count: %d", um->m_task_count);
    UTHREAD_MGR_TRACE_LOG("task_occupy: %d", um->m_task_occupy);
    UTHREAD_MGR_TRACE_LOG("finish_hint: %d", um->m_finish_hint);
    UTHREAD_MGR_TRACE_LOG("cur_task_index: %d", um->m_cur_task_index);
    if (is_dump_task) {
        for (i = 0; i < um->m_task_count; i++) {
            UTHREAD_MGR_TRACE_LOG("+++++");
            ut = &um->m_task[i];
            UTHREAD_MGR_TRACE_LOG("index: %d", i);
            UTHREAD_MGR_TRACE_LOG("func: %p", ut->m_func);
            UTHREAD_MGR_TRACE_LOG("arg: %p", ut->m_arg);
            UTHREAD_MGR_TRACE_LOG("status: %d", ut->m_status);
            UTHREAD_MGR_TRACE_LOG("ctx.uc_link: %p", ut->m_ctx.uc_link);
            UTHREAD_MGR_TRACE_LOG("main_index: %d", ut->m_main_index);
            UTHREAD_MGR_TRACE_LOG("+++++");
        }
    }
    UTHREAD_MGR_TRACE_LOG("=======");
    UTHREAD_MGR_TRACE_LOG();
}

#if 1
#include <assert.h>

int id_1 = 0, id_2 = 0;
void* _func_1(uthread_mgr_t* um, void *arg)
{
    if (!um) return um;

    int i = 1;
    assert((void*)9 == arg);
    MY_PRINTF("func_1: %d", i++);
    uthread_mgr_resume(um, id_2);
    uthread_mgr_yeild(um);
    uthread_mgr_resume(um, id_2);
    MY_PRINTF("func_1: %d", i++);
    /* uthread_mgr_dump(um, true); */
    return um;
}

void* _func_2(uthread_mgr_t *um, void *arg)
{
    if (!um) return um;
    int i = 1;

    assert((void*)10 == arg);
    MY_PRINTF("func_2: %d", i++);
    uthread_mgr_yeild(um);
    MY_PRINTF("func_2: %d", i++);
    /* uthread_mgr_dump(um, true); */
    return um;
}

int main()
{
    uthread_mgr_t *um = NULL;

    uthread_mgr_init(NULL, NULL);
    assert((um = uthread_mgr_new(2)));
    assert((id_1 = uthread_mgr_add(um, _func_1, (void*)9)) >= 0);
    assert((id_2 = uthread_mgr_add(um, _func_2, (void*)10)) >= 0);
    uthread_mgr_dump(um, false);

    while (!uthread_mgr_finish(um)) {
        uthread_mgr_resume(um, id_1);
        MY_PRINTF("main");
        /* uthread_mgr_resume(um, id_2); */
    }

    uthread_mgr_dump(um, true);
    uthread_mgr_free(um);
    return 0;
}

#endif
