/* @desc:
 *	用途：
 *		1. 在多线程的环境里，控制同一个函数仅能在一个线程中运行，其他线程只能等待该函数执行完才能再次执行该函数；
 *		2. 一个函数只允许执行N次
 *	注意：
 *		销毁管理器是非线程安全的
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "sync_n_mgr.h"


#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define SYNC_N_MGR_TRACE_LOG MY_PRINTF
#define SYNC_N_MGR_DEBUG_LOG MY_PRINTF
#define SYNC_N_MGR_INFO_LOG MY_PRINTF
#define SYNC_N_MGR_WARN_LOG MY_PRINTF
#define SYNC_N_MGR_ERROR_LOG MY_PRINTF

static sync_n_mgr_alloc_t g_sm_alloc = NULL;
static sync_n_mgr_free_t g_sm_free = NULL;

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

/* @func:
 *	初始化管理器
 */
void sync_n_mgr_init(sync_n_mgr_alloc_t alloc, sync_n_mgr_free_t dealloc)
{
	if (!alloc || !dealloc) g_sm_alloc = _malloc2calloc, g_sm_free = free;
	else g_sm_alloc = alloc, g_sm_free = dealloc;
}


/* @func:
 *	创建管理器
 */
sync_n_mgr_t* sync_n_mgr_new(size_t run_count, sync_n_mgr_func_t func, void *arg)
{
	if (!run_count || !func) return NULL;
	sync_n_mgr_t *sm = NULL;
	
	if (!(sm = g_sm_alloc(sizeof(sync_n_mgr_t)))) {
		SYNC_N_MGR_ERROR_LOG("g_sm_alloc error, errno: %d - %s", errno, strerror(errno));
		return NULL;
	}
	
	if (pthread_mutex_init(&sm->m_mutex, NULL)) {
		SYNC_N_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}
	
	sm->m_func = func, sm->m_arg = arg, sm->m_run_count = run_count;
	return sm;
	
err:
	if (sm) g_sm_free(sm);
	return NULL;
}


/* @func:
 *	销毁管理器
 */
void sync_n_mgr_free(sync_n_mgr_t *sm)
{
	if (!sm) return ;

	pthread_mutex_lock(&sm->m_mutex);
	pthread_mutex_destroy(&sm->m_mutex);
	g_sm_free(sm);
}


/* @func:
 *	执行管理器关联的函数
 */
void* sync_n_mgr_do(sync_n_mgr_t *sm)
{
	if (!sm) return NULL;
	void *ret = NULL;

	if (0 == __sync_fetch_and_add(&sm->m_run_count, 0)) return NULL;

	pthread_mutex_lock(&sm->m_mutex);
	if (0 == sm->m_run_count) goto out;
	sm->m_run_count--;
	ret = sm->m_func(sm->m_arg);

out:
	pthread_mutex_unlock(&sm->m_mutex);
	return ret;
	
	__sync_synchronize();
}

/* @func:
 *	打印管理器信息
 */
void sync_n_mgr_dump(sync_n_mgr_t *sm)
{
	if (!sm) return ;
	
	pthread_mutex_lock(&sm->m_mutex);
	SYNC_N_MGR_TRACE_LOG("=========");
	SYNC_N_MGR_TRACE_LOG("run_count: %lu", sm->m_run_count);
	SYNC_N_MGR_TRACE_LOG("func: %p", sm->m_func);
	SYNC_N_MGR_TRACE_LOG("arg: %p", sm->m_arg);
	SYNC_N_MGR_TRACE_LOG("=========");
	pthread_mutex_unlock(&sm->m_mutex);
}


#if 1
#include <assert.h>

#define run_count 20

size_t sum = 0;
size_t num = 0;

void* _func(void *arg)
{
	num++;
	sum += num;
	return arg;
}

void* _test_sync_n_mgr_do(void *arg)
{	
	if (!arg) return NULL;
	sync_n_mgr_t *sm = (sync_n_mgr_t*)arg;
	
	return sync_n_mgr_do(sm);
}

int main()
{
	sync_n_mgr_t *sm = NULL;
	size_t i = 0, j = 0, test_count = 0;
	pthread_t *pt = NULL;
	
	sync_n_mgr_init(NULL, NULL);

	for (test_count = 6; test_count > 0; test_count--) {
		assert((sm = sync_n_mgr_new(run_count, _func, NULL)));
		sync_n_mgr_dump(sm);
	
		assert((pt = calloc(run_count * test_count, sizeof(pthread_t))));
		
		for (i = 0; i < run_count * test_count; i++) {
			pthread_create(&pt[i], NULL, _test_sync_n_mgr_do, sm);
		}

		for (i = 0; i < run_count * test_count; i++) {
			pthread_join(pt[i], NULL);
		}

		for (i = 1; i <= run_count; i++) {
			j += i;
		}
		
		assert(j == sum);
		sync_n_mgr_free(sm);
		free(pt);
		MY_PRINTF("sum: %lu", sum);
	
		j = sum = num = 0;
		sm = NULL;
		pt = NULL;
	}
	
	MY_PRINTF("ok");
	return 0;
}

#endif
