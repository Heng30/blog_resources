/* @desc:
 *	1. 用于全局唯一索引关联的资源管理，如：套接字的资源管理
 *	2. 采用引用计数的方式，在引用计数为0时对资源进行释放
 *	3. 调用者需要保证:
 *		3.1 get后一定要ret
 *		3.2 在reference != 0时，调用new会造成内存泄露
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>

#include "global_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define GLOBAL_MGR_TRACE_LOG MY_PRINTF
#define GLOBAL_MGR_DEBUG_LOG MY_PRINTF
#define GLOBAL_MGR_INFO_LOG MY_PRINTF
#define GLOBAL_MGR_WARN_LOG MY_PRINTF
#define GLOBAL_MGR_ERROR_LOG MY_PRINTF

/* 全局节点管理器，一般用于套接字资源管理 */
typedef struct _global_mgr {
	global_mgr_member_t *m_member;
	size_t m_reference;
} global_mgr_t;

/* 内存管理函数 */
static global_mgr_alloc_t g_global_alloc = NULL;
static global_mgr_free_t g_global_free = NULL;
static global_mgr_alloc_t g_member_alloc = NULL;
static global_mgr_free_t g_member_free = NULL;

static size_t g_global_max_size = 0; /* 管理器大小 */
static pthread_mutex_t g_mutex;
static global_mgr_t *g_global_mgr = NULL;

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

void global_mgr_dump(size_t index)
{
	if (!g_global_mgr || index >= g_global_max_size) return ;
	
	GLOBAL_MGR_TRACE_LOG("==============");
	GLOBAL_MGR_TRACE_LOG("member: %p", g_global_mgr[index].m_member);
	GLOBAL_MGR_TRACE_LOG("reference: %lu", g_global_mgr[index].m_reference);
	GLOBAL_MGR_TRACE_LOG("==============");
}

void global_mgr_dump_all(void)
{
	if (!g_global_mgr) return ;
	size_t i = 0; 
	for (i = 0; i < g_global_max_size; i++) {
		GLOBAL_MGR_TRACE_LOG("==============");
		GLOBAL_MGR_TRACE_LOG("index: %lu", i);
		GLOBAL_MGR_TRACE_LOG("member: %p", g_global_mgr[i].m_member);
		GLOBAL_MGR_TRACE_LOG("reference: %lu", g_global_mgr[i].m_reference);
		GLOBAL_MGR_TRACE_LOG("==============");
	}
}

/* @func:
 *	初始化管理器
 */
bool global_mgr_init(size_t size, global_mgr_alloc_t global_alloc, global_mgr_free_t global_free,
								global_mgr_alloc_t member_alloc, global_mgr_free_t member_free)
{
	if (!size) return false;
	
	if (!global_alloc || !global_free) g_global_alloc = _malloc2calloc, g_global_free = free;
	else g_global_alloc = global_alloc, g_global_free = global_free;
	if (!member_alloc || !member_free) g_member_alloc = _malloc2calloc, g_member_free = free;
	else g_member_alloc = member_alloc, g_member_free = member_free;
	
	if (pthread_mutex_init(&g_mutex, NULL)) {
		GLOBAL_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
		return false;
	}
	
	if (!(g_global_mgr = (global_mgr_t*)g_global_alloc(size * sizeof(global_mgr_t)))) {
		GLOBAL_MGR_ERROR_LOG("g_global_alloc error, errno: %d - %s", errno, strerror(errno));
		return false;
	}
	g_global_max_size = size;
	return true;
}

/* @func:
 *	销毁管理器，仅用在程序退出时使用
 */
void global_mgr_destroy(void)
{
	size_t i = 0;
	
	pthread_mutex_lock(&g_mutex);
	for (i = 0; i < g_global_max_size && g_global_mgr; i++) 
		if (g_global_mgr[i].m_member) g_member_free(g_global_mgr[i].m_member);
	
	if (g_global_mgr) free(g_global_mgr);
	g_global_mgr = NULL; g_global_max_size = 0;
	g_global_alloc = NULL; g_global_free = NULL;
	g_member_alloc = NULL; g_member_free = NULL;
	pthread_mutex_unlock(&g_mutex);
	pthread_mutex_destroy(&g_mutex);
}

static bool _global_mgr_member_new(size_t index)
{
	global_mgr_member_t *gmm = NULL;
	if (!(gmm = (global_mgr_member_t*)g_member_alloc(sizeof(global_mgr_member_t)))) {
		GLOBAL_MGR_ERROR_LOG("g_member_alloc error,errno: %d - %s", errno, strerror(errno));
		return false;
	}
	
	if (g_global_mgr[index].m_reference != 0) {
		GLOBAL_MGR_WARN_LOG("old global_mgr_member will be cover");
		global_mgr_dump(index);
	}
	
	g_global_mgr[index].m_member = gmm;
	g_global_mgr[index].m_reference = 0;
	return true;
}

/* @func:
 *	分配一个管理节点的成员
 */
bool global_mgr_member_new(size_t index)
{
	if (!g_global_mgr || index >= g_global_max_size) return false;
	
	bool ret = false;
	pthread_mutex_lock(&g_mutex);
	ret = _global_mgr_member_new(index);
	pthread_mutex_unlock(&g_mutex);
	
	return ret;
}

/* @func:
 *	释放管理器成员占用的空间
 */
bool global_mgr_member_free(size_t index)
{
	if (!g_global_mgr || index >= g_global_max_size) return false;
	pthread_mutex_lock(&g_mutex);
	if (g_global_mgr[index].m_reference == 0) {
		if (g_global_mgr[index].m_member) g_member_free(g_global_mgr[index].m_member);
		memset(&g_global_mgr[index], 0, sizeof(g_global_mgr[index]));
	}
	pthread_mutex_unlock(&g_mutex);
	return true;
}

/* @func:
 *	获取一个管理器的成员
 */
global_mgr_member_t* global_mgr_member_get(size_t index)
{
	if (!g_global_mgr || index >= g_global_max_size) return NULL;
	global_mgr_member_t *gmm = NULL;
	
	pthread_mutex_lock(&g_mutex);
	if ((gmm = g_global_mgr[index].m_member))
		g_global_mgr[index].m_reference++;
	pthread_mutex_unlock(&g_mutex);
	return gmm;
}

/* @func:
 *	返还管理器的成员
 */
bool global_mgr_member_ret(size_t index)
{
	if (!g_global_mgr || index >= g_global_max_size) return false;
	bool ret = false;
	
	pthread_mutex_lock(&g_mutex);
	if(g_global_mgr[index].m_reference) {
		ret = true;
		g_global_mgr[index].m_reference--;
	}
	pthread_mutex_unlock(&g_mutex);
	
	return ret;
}

int main()
{
#include <assert.h>

	size_t i = 0, size = 10;
	pthread_t pt[20];
	global_mgr_member_t *gmm = NULL;
	
	assert(global_mgr_init(size, NULL, NULL, NULL, NULL));
	
	/* 单线程测试 */
	for (i = 0; i < size; i++) {
		assert(global_mgr_member_new(i));
		assert((gmm = global_mgr_member_get(i)));
		assert(global_mgr_member_ret(i));
		assert(global_mgr_member_free(i));
		assert(!(gmm = global_mgr_member_get(i)));
	}
	
	/* 多线程测试 */
	for (i = 0; i < size; i++) global_mgr_member_new(i);
	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
		pthread_create(&pt[i], NULL, ({
		void* _(void* arg) {
			size_t k = 0, j = 0;
			for (k = 0; k < 1024 * 100; k++) {
				for (j = 0; j < size; j++) {
					assert(global_mgr_member_get(j));
					assert(global_mgr_member_ret(j));
				}
			}
			return arg;
		}; _;}), NULL);
	}
	
	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
		pthread_join(pt[i], NULL);
	}
	
	for (i = 0; i < size; i++) {
		assert(global_mgr_member_free(i));
		assert(!(gmm = global_mgr_member_get(i)));
	}
		
	global_mgr_dump_all();
	global_mgr_destroy();
	assert(!global_mgr_member_new(0));
	MY_PRINTF("OK");
	return 0;
}
