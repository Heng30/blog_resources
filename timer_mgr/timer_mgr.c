#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "timer_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define TM_TRACE_LOG MY_PRINTF
#define TM_DEBUG_LOG MY_PRINTF
#define TM_INFO_LOG MY_PRINTF
#define TM_WARN_LOG MY_PRINTF
#define TM_ERROR_LOG MY_PRINTF

#define TIMER_MAX 32 /* 最大的定时器数目 */

static void* _malloc2calloc(size_t size);
static timer_mgr_alloc_t g_tm_alloc = _malloc2calloc;
static timer_mgr_free_t g_tm_free = free;
static timer_mgr_t *g_tm = NULL;
static size_t g_tm_max = TIMER_MAX;
static bool g_tm_stop = false; /* 是否停止定时器 */

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

static void _set_default(void)
{
	g_tm = NULL;
	g_tm_max = TIMER_MAX;
	g_tm_stop = false;
	g_tm_alloc = _malloc2calloc;
	g_tm_free = free;
}

static int _find_unused(void)
{
	if (!g_tm) return -1;
	size_t i = 0;
	
	for (i = 0; i < g_tm_max; i++)
		if (g_tm[i].m_interval == 0) return i;
		
	return -1;
}

/* @func:
 *	初始化管理器
 */
bool timer_mgr_init(size_t timer_max, timer_mgr_alloc_t alloc, timer_mgr_free_t dealloc)
{
	if (timer_max > 0) g_tm_max = timer_max;
	if (!alloc || !dealloc) g_tm_alloc = _malloc2calloc, g_tm_free = free;
	else g_tm_alloc = alloc, g_tm_free = dealloc;

	if (g_tm = g_tm_alloc(timer_max * sizeof(timer_mgr_t)), !g_tm) {
		TM_ERROR_LOG("g_tm_alloc error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}
	memset(g_tm, 0, timer_max * sizeof(timer_mgr_t));
	return true;
	
err:
	_set_default();
	return false;
}

/* @func:
 *	销毁管理器
 */
void timer_mgr_destroy(void)
{
	if (!g_tm) return ;
	g_tm_free(g_tm);
	_set_default();
}

/* @func:
 *	创建一个定时器
 * @return:
 *	-1: 错误
 *	大于-1: 定时任务唯一标识
 */
int timer_mgr_new(timer_mgr_cb_t cb, void* arg, int interval)
{
	if (!cb || interval <= 0) return -1;
	if (!g_tm) return -1;
	int id = 0;
	
	if (id = _find_unused(), id < 0) {
		TM_WARN_LOG("can't find free id to register new timer");
		return -1;
	}
	g_tm[id].m_cb = cb;
	g_tm[id].m_arg = arg;
	g_tm[id].m_interval = interval;
	g_tm[id].m_remain = 1;
	return id;
}

/* @func:
 *	通过id释放一个定时器
 */
void timer_mgr_free_by_id(size_t id)
{
	if (!g_tm || id >= g_tm_max) return;
	g_tm[id].m_interval = 0;
}

/* @func:
 *	通过一个回调函数释放一个定时器
 */
void timer_mgr_free_by_cb(timer_mgr_cb_t cb)
{
	if (!g_tm || !cb) return;
	size_t i = 0;
	
	for (i = 0; i < g_tm_max; i++)
		if (g_tm[i].m_cb == cb) break;
		
	if (i == g_tm_max) return ;
	g_tm[i].m_interval = 0;
}

static void* _timer_mgr_do(void *arg)
{
	(void)arg;
	size_t i = 0;
	
	while (true) {
		if (!g_tm) break;
		if (g_tm_stop) break;
		for (i = 0; i < g_tm_max; i++) {
			if (g_tm[i].m_interval == 0) continue;
			g_tm[i].m_remain %= g_tm[i].m_interval;
			if (g_tm[i].m_remain == 0) g_tm[i].m_cb(g_tm[i].m_arg);
			g_tm[i].m_remain++;
		}
		sleep(1);
	}
	timer_mgr_destroy();
	return NULL;
}

/* @func:
 *	定时调用回调函数
 */
void timer_mgr_dispatch(void)
{
	pthread_t pt;
	pthread_create(&pt, NULL, _timer_mgr_do, NULL);
}

/* @func:
 *	停止定时管理器，释放资源
 */
void timer_mgr_stop(void)
{
	g_tm_stop = true;
}

/* ====================Test=====================*/
#if 1
#include <assert.h>

static void* _cb_1(void *arg)
{
	static int cnt = 1;
	MY_PRINTF("I am _cb_1, count: %d", cnt++);
	return arg;
}

static void* _cb_2(void *arg)
{
	static int cnt = 1;
	MY_PRINTF("I am _cb_2, count: %d", cnt++);
	return arg;
}

static void* _cb_3(void *arg)
{
	static int cnt = 1;
	MY_PRINTF("I am _cb_3, count: %d", cnt++);
	return arg;
}

static void* _cb_4(void *arg)
{
	static int cnt = 1;
	MY_PRINTF("I am _cb_4, count: %d", cnt++);
	return arg;
}


int main()
{
	int id_1 = 0, id_2 = 0, id_3 = 0, id_4 = 0;
	int i = 0;
	
	assert(timer_mgr_init(3, NULL, NULL));
	assert((id_1 = timer_mgr_new(_cb_1, NULL, 2), id_1 >= 0));
	assert((id_2 = timer_mgr_new(_cb_2, NULL, 2), id_2 >= 0));
	assert((id_3 = timer_mgr_new(_cb_3, NULL, 2), id_3 >= 0));
	assert((id_4 = timer_mgr_new(_cb_4, NULL, 2), id_4 < 0));

	timer_mgr_dispatch();

	for (i = 0; i < 12; i++) {
		if (i == 10) break;
		if (i == 3) timer_mgr_free_by_id(id_1);
		if (i == 7) timer_mgr_free_by_cb(_cb_2);		
		sleep(1);
	}
	
	timer_mgr_stop();
	sleep(2);
	return 0;
}
#endif
