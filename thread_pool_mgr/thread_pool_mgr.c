#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "thread_pool_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define THREAD_POOL_MGR_TRACE_LOG MY_PRINTF
#define THREAD_POOL_MGR_DEBUG_LOG MY_PRINTF
#define THREAD_POOL_MGR_INFO_LOG MY_PRINTF
#define THREAD_POOL_MGR_WARN_LOG MY_PRINTF
#define THREAD_POOL_MGR_ERROR_LOG MY_PRINTF

typedef struct _thread_pool_mgr_task {
    thread_pool_mgr_task_func_t m_func;
    void *m_arg;
} thread_pool_mgr_task_t;

typedef struct thread_pool_mgr {
  pthread_mutex_t m_mutex;
  pthread_cond_t m_cond;
  pthread_t *m_thread; /* 线程数组 */
  thread_pool_mgr_task_t *m_task; /* 任务数组 */
  size_t m_thread_size; /* 线程数组大小 */
  size_t m_task_size; /* 任务数组大小 */
  size_t m_task_cur; /* 指向待执行的任务的数组下标 */
  size_t m_task_tail; /* 指向能够插入任务的数组下标 */
  size_t m_task_count; /* 当前待执行任务的数量 */
  bool m_is_shutdown; /* 关闭线程标志 */
  size_t m_unactive_thread; /* 可用线程的数量 */
} thread_pool_mgr_t;

static thread_pool_mgr_alloc_t g_tpm_alloc = NULL;
static thread_pool_mgr_free_t g_tpm_free = NULL;
static thread_pool_mgr_t *g_tpm = NULL;

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

/* @func:
 *	释放占用的内存
 */
static bool _thread_pool_mgr_free(void)
{
    if(!g_tpm) return false;

	pthread_mutex_lock(&(g_tpm->m_mutex));
	if (g_tpm->m_thread) g_tpm_free(g_tpm->m_thread);
	if (g_tpm->m_task) g_tpm_free(g_tpm->m_task);
	pthread_mutex_destroy(&g_tpm->m_mutex);
	pthread_cond_destroy(&g_tpm->m_cond);
    g_tpm_free(g_tpm);
	g_tpm = NULL;
    return true;
}

/* @func:
 *	工作线程
 */
static void* thread_pool_mgr_thread(void *arg)
{
    thread_pool_mgr_task_t task;

    while (true) {
        pthread_mutex_lock(&g_tpm->m_mutex);
        while((g_tpm->m_task_count == 0) && (!g_tpm->m_is_shutdown)) {
            pthread_cond_wait(&g_tpm->m_cond, &g_tpm->m_mutex);
        }

        if(g_tpm->m_is_shutdown) goto err;

        if (g_tpm->m_task_cur >= g_tpm->m_task_size) {
			THREAD_POOL_MGR_WARN_LOG("task_cur out of range, task_cur: %lu, task_size: %lu", g_tpm->m_task_cur, g_tpm->m_task_size);
			g_tpm->m_task_cur = 0;
			g_tpm->m_task_tail = 0;
			g_tpm->m_task_count = 0;
            pthread_mutex_unlock(&g_tpm->m_mutex);
            continue;
		}

        task.m_func = g_tpm->m_task[g_tpm->m_task_cur].m_func;
        task.m_arg = g_tpm->m_task[g_tpm->m_task_cur].m_arg;
		g_tpm->m_task_cur++;
        g_tpm->m_task_cur %= g_tpm->m_task_size;
        g_tpm->m_task_count--;
		g_tpm->m_unactive_thread--;
        pthread_mutex_unlock(&g_tpm->m_mutex);

        task.m_func(task.m_arg);

		pthread_mutex_lock(&g_tpm->m_mutex);
		g_tpm->m_unactive_thread++;
		pthread_mutex_unlock(&g_tpm->m_mutex);
    }
    return arg;

err:
    pthread_mutex_unlock(&g_tpm->m_mutex);
    pthread_exit(NULL);
    return NULL;
}


/* @func:
 *	初始化线程池
 */
bool thread_pool_mgr_init(size_t thread_size, size_t task_size,
					thread_pool_mgr_alloc_t alloc, thread_pool_mgr_free_t dealloc)
{
	if (!thread_size || !task_size) return false;
    size_t i = 0;

	if (!alloc || !dealloc) g_tpm_alloc = _malloc2calloc, g_tpm_free = free;
	else g_tpm_alloc = alloc, g_tpm_free = dealloc;

    if(!(g_tpm = (thread_pool_mgr_t*)g_tpm_alloc(sizeof(thread_pool_mgr_t)))) {
		THREAD_POOL_MGR_ERROR_LOG("g_tpm_alloc error, errno: %d - %s", errno, strerror(errno));
        return false;
    }

	if(pthread_mutex_init(&g_tpm->m_mutex, NULL)){
		THREAD_POOL_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
		free(g_tpm); g_tpm = NULL;
		return false;
	}

	if (pthread_cond_init(&g_tpm->m_cond, NULL)) {
		THREAD_POOL_MGR_ERROR_LOG("pthread_cond_init error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

    g_tpm->m_thread = (pthread_t*)g_tpm_alloc(sizeof(pthread_t) * thread_size);
    g_tpm->m_task = (thread_pool_mgr_task_t*)g_tpm_alloc(sizeof(thread_pool_mgr_task_t) * task_size);

	if (!g_tpm->m_thread || !g_tpm->m_task) {
		THREAD_POOL_MGR_ERROR_LOG("g_tmp_alloc error, errno: %d - %s", errno, strerror(errno));
        goto err;
    }

    for(i = 0; i < thread_size; i++) {
        if(pthread_create(&g_tpm->m_thread[i], NULL, thread_pool_mgr_thread, NULL)) {
			THREAD_POOL_MGR_ERROR_LOG("pthread_create error, errno: %d - %s", errno, strerror(errno));
            thread_pool_mgr_destroy();
			return false;
        }
    }

	g_tpm->m_thread_size = thread_size;
	g_tpm->m_unactive_thread = thread_size;
	g_tpm->m_task_size = task_size;
    return true;

err:
	_thread_pool_mgr_free();
	g_tpm = NULL;
    return false;
}

/* func:
 *	添加任务
 */
bool thread_pool_mgr_task_add(thread_pool_mgr_task_func_t func, void *arg)
{
    if(!g_tpm || !func) return false;

    pthread_mutex_lock(&g_tpm->m_mutex);
	if(g_tpm->m_is_shutdown) goto err;
	if(g_tpm->m_task_count == g_tpm->m_task_size) {
		THREAD_POOL_MGR_WARN_LOG("task queue is full, task count: %lu", g_tpm->m_task_count);
		goto err;
	}

	if (g_tpm->m_task_tail >= g_tpm->m_task_size) {
		THREAD_POOL_MGR_WARN_LOG("task tail out of range, task_tail: %lu, task_size: %lu",
				g_tpm->m_task_tail, g_tpm->m_task_size);
		g_tpm->m_task_cur = 0;
		g_tpm->m_task_tail = 0;
		g_tpm->m_task_count = 0;
	}

	g_tpm->m_task[g_tpm->m_task_tail].m_func = func;
	g_tpm->m_task[g_tpm->m_task_tail].m_arg = arg;
	g_tpm->m_task_tail++;
	g_tpm->m_task_tail %= g_tpm->m_task_size;
	g_tpm->m_task_count++;
	pthread_mutex_unlock(&g_tpm->m_mutex);

	if(pthread_cond_signal(&g_tpm->m_cond)) {
		THREAD_POOL_MGR_WARN_LOG("pthread_cond_signal error, errno: %d - %s", errno, strerror(errno));
		return false;
	}
	return true;

err:
	pthread_mutex_unlock(&g_tpm->m_mutex);
	return false;
}


/* @func:
 *	 销毁线程池占用的内存
 */
bool thread_pool_mgr_destroy(void)
{
	if(!g_tpm) return false;
    size_t i = 0;;

    pthread_mutex_lock(&g_tpm->m_mutex);
    g_tpm->m_is_shutdown = true;;
	pthread_mutex_unlock(&g_tpm->m_mutex);

	if(pthread_cond_broadcast(&g_tpm->m_cond)) {
	   THREAD_POOL_MGR_WARN_LOG("pthread_cond_broadcast error, errno: %d - %s", errno, strerror(errno));
	   goto exit_free;
	}

	for(i = 0; i < g_tpm->m_thread_size; i++) {
		if(pthread_join(g_tpm->m_thread[i], NULL)) {
			THREAD_POOL_MGR_WARN_LOG("pthread_join error, errno: %d - %s", errno, strerror(errno));
			goto exit_free;
		}
	}
	_thread_pool_mgr_free();
	return true;

exit_free:
    _thread_pool_mgr_free();
    return false;
}

/* @func:
 *  等待所有线程返回
 */
bool thread_pool_mgr_dispatch(void)
{
    if (!g_tpm) return false;
    size_t i = 0;
	for(i = 0; i < g_tpm->m_thread_size; i++) {
		if(pthread_join(g_tpm->m_thread[i], NULL)) {
			THREAD_POOL_MGR_WARN_LOG("pthread_join error, errno: %d - %s", errno, strerror(errno));
            return false;
		}
    }

    return true;
}

/* @func:
 *  打印信息
 */
void thread_pool_mgr_dump(void)
{
    if (!g_tpm) return ;

    pthread_mutex_lock(&g_tpm->m_mutex);
    THREAD_POOL_MGR_TRACE_LOG("===============");
    THREAD_POOL_MGR_TRACE_LOG("thread: %p", g_tpm->m_thread);
    THREAD_POOL_MGR_TRACE_LOG("task: %p", g_tpm->m_task);
    THREAD_POOL_MGR_TRACE_LOG("thread_size: %lu", g_tpm->m_thread_size);
    THREAD_POOL_MGR_TRACE_LOG("task_size: %lu", g_tpm->m_task_size);
    THREAD_POOL_MGR_TRACE_LOG("task_cur: %lu", g_tpm->m_task_cur);
    THREAD_POOL_MGR_TRACE_LOG("task_tail: %lu", g_tpm->m_task_tail);
    THREAD_POOL_MGR_TRACE_LOG("task_count: %lu", g_tpm->m_task_count);
    THREAD_POOL_MGR_TRACE_LOG("is_shutdown: %d", g_tpm->m_is_shutdown);
    THREAD_POOL_MGR_TRACE_LOG("unactive_thread: %lu", g_tpm->m_unactive_thread);
    THREAD_POOL_MGR_TRACE_LOG("===============");
    pthread_mutex_unlock(&g_tpm->m_mutex);
}

int main()
{
#include <assert.h>
    int i = 0;
    int index = 0;
    assert(thread_pool_mgr_init(50, 100, NULL, NULL));

    for (i = 0; i < 1024 ; i++) {
        thread_pool_mgr_task_add(({
                    void* _(void *arg) {
                    int j = 0;
                    int count = 0;
                    size_t sum = 0;
                    for (j = 0; j < 1024 * 1024; j++) sum += j;
                    count = __sync_add_and_fetch(&index, 1);
                    MY_PRINTF("index: %d, sum: %lu", count, sum);
                    return NULL;
                    }; _;}), NULL);
    }

    /* assert(thread_pool_mgr_dispatch()); */
    sleep(3);
    thread_pool_mgr_dump();
    assert(thread_pool_mgr_destroy());
    MY_PRINTF("OK");
    return 0;
}
