#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "thread_ctrller.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define _TRACE_LOG MY_PRINTF
#define _DEBUG_LOG MY_PRINTF
#define _INFO_LOG MY_PRINTF
#define _WARN_LOG MY_PRINTF
#define _ERROR_LOG MY_PRINTF

#define _HIGH_WATER 1024

#define ADD_ITEM(head, tail, item) do { \
    if (!(head) || !(tail)) (head) = (tail) = (item); \
    else (tail)->m_next = (item), (tail) = (item); \
} while(0)

#define DEL_ITEM(head, tail) ({ \
    typeof(head) _ = (head); \
    if ((head) == (tail)) (head) = (tail) = NULL; \
    else (head) = (head)->m_next; \
    _; \
})

/* 回调函数节点 */
typedef struct _thread_ctrller_item {
    thread_ctrller_cb_t m_cb;
    void *m_arg;
    struct _thread_ctrller_item *m_next;
} thread_ctrller_item_t;

/* 控制器节点 */
typedef struct _thread_ctrller {
    unsigned char m_status; /* 状态 */
    size_t m_high_water; /* 高水位 */
    size_t m_item_cnt; /* 队列深度 */
    pthread_t m_pt;
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
    thread_ctrller_item_t *m_head, *m_tail;
} thread_ctrller_t;


typedef void* (*g_alloc_t) (size_t size);
typedef void (*g_free_t) (void *ptr);
static void* _malloc2calloc(size_t size);
static void _free(void *ptr);

static g_alloc_t g_alloc = _malloc2calloc;
static g_free_t g_free = _free;
static bool g_is_destroy = false;
static int g_thread_cnt = 5; /* 线程数量 */
static thread_ctrller_t *g_head = NULL;


static void* _malloc2calloc(size_t size)
{
    return calloc(1, size);
}

static void _free(void *ptr)
{
    if (!ptr) return;
    free(ptr);
}

static void _set_default(void)
{
    g_alloc = _malloc2calloc;
    g_free = _free;
    g_thread_cnt = 5;
    g_head = NULL;
    g_is_destroy = false;
}

/* @func:
 *  初始化
 * @ret:
 *  大于0：控制器节点数量
 *  -1: 出错
 */
int thread_ctrller_init(int thread_cnt)
{
    int i = 0;
    if (thread_cnt <= 0) g_thread_cnt = 5;
    else g_thread_cnt = thread_cnt;

    if (g_head = g_alloc(g_thread_cnt * sizeof(thread_ctrller_t)), !g_head) {
        _ERROR_LOG("g_head error, errno: %d - %s", errno, strerror(errno));
        goto err;
    }

    for (i = 0; i < g_thread_cnt; i++) {
        memset(&g_head[i], 0, sizeof(g_head[i]));
        g_head[i].m_high_water = _HIGH_WATER;
        g_head[i].m_status = THREAD_CTRLLER_STATUS_SHARE;
        if (pthread_cond_init(&g_head[i].m_cond, NULL)) {
            _ERROR_LOG("pthread_cond_init error, errno: %d - %s", errno, strerror(errno));
            goto err;
        }

        if (pthread_mutex_init(&g_head[i].m_mutex, NULL)) {
            _ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
            goto err;

        }
    }

    return g_thread_cnt;
err:
    if (g_head) g_free(g_head);
    _set_default();
    return -1;
}

/* @func:
 *  设置一个线程
 */
bool thread_ctrller_set(int id, size_t high_water, unsigned char status)
{
    if (!high_water || id < 0 || id >= g_thread_cnt || !g_head) return false;

    g_head[id].m_high_water = high_water;
    g_head[id].m_status = status;
    return true;
}

/* @func
 *  向一个线程添加任务
 * @id:
 *  id不在范围内，会选择一个深度最浅的共享线程添加任务
 */
bool thread_ctrller_add(int id, thread_ctrller_cb_t cb, void *arg)
{
    if (!cb || !g_head) return false;
    if (g_is_destroy) return false;
    size_t item_cnt = (size_t)-1;
    thread_ctrller_item_t *item = NULL;
    int i = 0;

    if (id >= 0 && id < g_thread_cnt) goto set;

    /* 查找共享的线程，并且队列深度最浅的线程 */
    for (i = 0; i < g_thread_cnt; i++) {
        if (g_head[i].m_status & THREAD_CTRLLER_STATUS_UNSHARE) continue;
        if (g_head[i].m_item_cnt >= g_head[i].m_high_water) continue;
        if (g_head[i].m_item_cnt < item_cnt) {
            item_cnt = g_head[i].m_item_cnt;
            id = i;
            if (item_cnt == 0) break;
        }
    }
    if (id < 0 || id >= g_thread_cnt) return false;

set:
    if (g_head[id].m_item_cnt >= g_head[id].m_high_water) return false;

    if (item = g_alloc(sizeof(*item)), !item) {
       _WARN_LOG("g_alloc error, errno: %d - %s", errno, strerror(errno));
       return false;
    }

    item->m_cb = cb;
    item->m_arg = arg;

    pthread_mutex_lock(&g_head[id].m_mutex);
    ADD_ITEM(g_head[id].m_head, g_head[id].m_tail, item);
    g_head[id].m_item_cnt++;
    if (pthread_cond_signal(&g_head[id].m_cond))
        _WARN_LOG("pthread_cond_signal error, errno: %d - %s", errno, strerror(errno));
    pthread_mutex_unlock(&g_head[id].m_mutex);

    return true;
}

/* @func:
 *  工作线程
 */
static void* _thread_wait(void *arg)
{
    if (!arg) return NULL;
    thread_ctrller_t *tc = (thread_ctrller_t*)arg;
    thread_ctrller_item_t *item = NULL;
    thread_ctrller_cb_t cb = NULL;
    void *cb_arg = NULL;

    while (true) {
        pthread_mutex_lock(&tc->m_mutex);
        while((!tc->m_head) && (!g_is_destroy))
            pthread_cond_wait(&tc->m_cond, &tc->m_mutex);

        /* 所有任务完成，并且设置了退出，线程才退出 */
        if(g_is_destroy && (!tc->m_head)) break;

        item = DEL_ITEM(tc->m_head, tc->m_tail);
        if (!item) goto next;

        cb = item->m_cb;
        cb_arg = item->m_arg;
        tc->m_item_cnt--;
next:
        if (!tc->m_head) tc->m_item_cnt = 0; /* 不必要的检查 */
        pthread_mutex_unlock(&tc->m_mutex);

        if (cb) cb(cb_arg);
        cb = NULL, cb_arg = NULL;
        if (item) g_free(item);
    }

    pthread_mutex_unlock(&tc->m_mutex);
    pthread_exit(NULL);
    return NULL;
}

/* @func:
 *  启动线程控制器
 */
bool thread_ctrller_dispatch(void)
{
    if (!g_head) return false;
    if (g_is_destroy) return false;
    int i = 0;

    for(i = 0; i < g_thread_cnt; i++) {
        if(pthread_create(&g_head[i].m_pt, NULL, _thread_wait, (void*)&g_head[i])) {
            _ERROR_LOG("pthread_create error, errno: %d - %s", errno, strerror(errno));
            thread_ctrller_destroy();
            return false;
        }
    }
    return true;
}

/* @func:
 *  等待所有线程返回
 */
static void _thread_ctrller_join(void)
{
    if (!g_head) return ;
    int i = 0;

    for (i = 0; i < g_thread_cnt; i++) {
        if (pthread_join(g_head[i].m_pt, NULL)) {
            _WARN_LOG("pthread_join error, errno: %d - %s", errno, strerror(errno));
            return ;
        }
    }
}

/* @func:
 *  销毁线程控制器
 */
void thread_ctrller_destroy(void)
{
    if (!g_head) return ;
    g_is_destroy = true;
    int i = 0;

    for (i = 0; i < g_thread_cnt; i++) {
        pthread_mutex_lock(&g_head[i].m_mutex);
        if (pthread_cond_signal(&g_head[i].m_cond))
            _WARN_LOG("pthread_cond_signal error, errno: %d - %s", errno, strerror(errno));
        pthread_mutex_unlock(&g_head[i].m_mutex);
    }

    _thread_ctrller_join();
    for (i = 0; i < g_thread_cnt; i++) {
        pthread_mutex_destroy(&g_head[i].m_mutex);
        pthread_cond_destroy(&g_head[i].m_cond);
    }

    g_free(g_head);
    _set_default();
}

/* @func:
 *  打印一个节点
 */
void thread_ctrller_dump(int id)
{
    if (!g_head || id < 0 || id >= g_thread_cnt) return ;

    _TRACE_LOG("==============");
    _TRACE_LOG("status: %#x", g_head[id].m_status);
    _TRACE_LOG("high_water: %lu", g_head[id].m_high_water);
    _TRACE_LOG("item_cnt: %lu", g_head[id].m_item_cnt);
    _TRACE_LOG("head: %p", g_head[id].m_head);
    _TRACE_LOG("tail: %p", g_head[id].m_tail);
    _TRACE_LOG("==============");
}

/* @func:
 *  打印所有节点
 */
void thread_ctrller_dump_all(void)
{
    int i = 0;
    _TRACE_LOG("+++++++++++++++");
    _TRACE_LOG("g_is_destroy: %d", g_is_destroy);
    _TRACE_LOG("g_thread_cnt: %d", g_thread_cnt);
    _TRACE_LOG("g_head: %p", g_head);
    _TRACE_LOG("+++++++++++++++");
    for (i = 0; i < g_thread_cnt; i++)
        thread_ctrller_dump(i);
}

#if 1

#include <assert.h>

void* print_1(void *arg)
{
    printf("print_1\n");
    return arg;
}

void* print_2(void *arg)
{
    printf("print_2\n");
    return arg;
}

void* print_3(void *arg)
{
    printf("print_3\n");
    return arg;
}

int main()
{
    int cnt = 3, i = 0;
    assert((cnt = thread_ctrller_init(cnt)) == 3);
    assert(thread_ctrller_set(0, 3, THREAD_CTRLLER_STATUS_UNSHARE));
    assert(thread_ctrller_set(1, 2, THREAD_CTRLLER_STATUS_SHARE));
    assert(thread_ctrller_set(2, 3, THREAD_CTRLLER_STATUS_SHARE));
    assert(!thread_ctrller_set(3, 3, THREAD_CTRLLER_STATUS_UNSHARE));

    for (i = 0; i < 3; i++)
        assert(thread_ctrller_add(0, print_1, NULL));
    assert(!thread_ctrller_add(0, print_1, NULL));

    assert(thread_ctrller_add(-1, print_2, NULL));
    assert(thread_ctrller_add(1, print_2, NULL));
    assert(thread_ctrller_add(cnt, print_3, NULL));
    assert(thread_ctrller_add(-1, print_2, NULL));
    assert(thread_ctrller_add(-1, print_2, NULL));
    assert(!thread_ctrller_add(-1, print_2, NULL));
    thread_ctrller_dump_all();
    thread_ctrller_dispatch();

    thread_ctrller_dump_all();
    thread_ctrller_destroy();
    return 0;
}

#endif
