#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>

#include "hash_bucket_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define HASH_BUCKET_MGR_TRACE_LOG MY_PRINTF
#define HASH_BUCKET_MGR_DEBUG_LOG MY_PRINTF
#define HASH_BUCKET_MGR_INFO_LOG MY_PRINTF
#define HASH_BUCKET_MGR_WARN_LOG MY_PRINTF
#define HASH_BUCKET_MGR_ERROR_LOG MY_PRINTF

#define HASH_MGR_MEMBER_INIT_CAPACITY 0XF

/* 内存管理函数 */
static hash_bucket_mgr_alloc_t g_hm_alloc = NULL;
static hash_bucket_mgr_free_t g_hm_free = NULL;

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

/* @func:
 *	打印一个管理节点
 */
void hash_bucket_mgr_dump(hash_bucket_mgr_t *hm, bool is_dump_member)
{
	if (!hm) return ;
	hash_bucket_mgr_member_t *hmm = NULL;
	size_t i = 0, index = 0;
	bool print_line = false;
	
	pthread_mutex_lock(&hm->m_mutex);
	HASH_BUCKET_MGR_TRACE_LOG("==============");
	HASH_BUCKET_MGR_TRACE_LOG("capacity: %lu", hm->m_capacity);
	HASH_BUCKET_MGR_TRACE_LOG("hash_func: %p", hm->m_hash_func);
	HASH_BUCKET_MGR_TRACE_LOG("compare_func: %p", hm->m_compare_func);
	HASH_BUCKET_MGR_TRACE_LOG("member: %p", hm->m_member);
	HASH_BUCKET_MGR_TRACE_LOG("");
	
	if (is_dump_member) {
		for (i = 0; i < hm->m_capacity; i++) {
			print_line = false;
			for (hmm = hm->m_member[i], index = 1; hmm; hmm = hmm->m_next, index++) {
				HASH_BUCKET_MGR_TRACE_LOG("hash bucket index: %lu", i);
				HASH_BUCKET_MGR_TRACE_LOG("linker index: %lu", index);
				HASH_BUCKET_MGR_TRACE_LOG("ptr: %p", hmm->m_ptr);
			//	HASH_BUCKET_MGR_TRACE_LOG("ptr_value: %lu", *(size_t*)hmm->m_ptr);
				HASH_BUCKET_MGR_TRACE_LOG("prev: %p", hmm->m_prev);
				HASH_BUCKET_MGR_TRACE_LOG("self: %p", hmm);
				HASH_BUCKET_MGR_TRACE_LOG("next: %p", hmm->m_next);
				print_line = true;
			}
			if (print_line) HASH_BUCKET_MGR_TRACE_LOG("");
		}
	}
	HASH_BUCKET_MGR_TRACE_LOG("==============");
	pthread_mutex_unlock(&hm->m_mutex);
}

/* @func:
 *	初始化管理器
 */
void hash_bucket_mgr_init(hash_bucket_mgr_alloc_t hash_bucket_alloc, hash_bucket_mgr_free_t hash_bucket_free)
{
	if (!hash_bucket_alloc || !hash_bucket_free) g_hm_alloc = _malloc2calloc, g_hm_free = free;
	else g_hm_alloc = hash_bucket_alloc, g_hm_free = hash_bucket_free;
}

/* @func:
 *	创建一个hash管理节点
 */
hash_bucket_mgr_t* hash_bucket_mgr_new(size_t capacity, hash_bucket_hash_func_t hash_func, 
								hash_bucket_compare_func_t compare_func)
{
	if (!hash_func || !compare_func) return NULL;
	if (!capacity) capacity = HASH_MGR_MEMBER_INIT_CAPACITY;
	hash_bucket_mgr_t *hm = NULL;
	
	if (!(hm = (hash_bucket_mgr_t*)g_hm_alloc(sizeof(hash_bucket_mgr_t)))) {
		HASH_BUCKET_MGR_ERROR_LOG("g_hm_alloc error,errno: %d - %s", errno, strerror(errno));
		return false;
	}
	if (!(hm->m_member = (hash_bucket_mgr_member_t**)g_hm_alloc(capacity * sizeof(hash_bucket_mgr_member_t*)))) {
		HASH_BUCKET_MGR_ERROR_LOG("g_hm_alloc error, errno: %d - %s", errno, strerror(errno));
		goto free_exit;
	}
	
	if (pthread_mutex_init(&hm->m_mutex, NULL)) {
		HASH_BUCKET_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
		goto free_exit;
	}
	
	hm->m_capacity = capacity;
	hm->m_compare_func = compare_func;
	hm->m_hash_func = hash_func;
	return hm;
	
free_exit:
	if (hm) {
		if (hm->m_member) g_hm_free(hm->m_member);
		g_hm_free(hm);
	}
	return NULL;
}

/* @func:
 *	销毁一个管理节点，但不销毁真正ptr节点的内存
 */
void hash_bucket_mgr_free(hash_bucket_mgr_t *hm)
{
	if (!hm) return ;
	pthread_mutex_lock(&hm->m_mutex);
	if (hm->m_member) g_hm_free(hm->m_member);
	pthread_mutex_destroy(&hm->m_mutex);
	g_hm_free(hm);
}

static bool _hash_bucket_mgr_member_add(hash_bucket_mgr_t *hm, void *ptr)
{
	if (!hm || !ptr) return false;
	hash_bucket_mgr_member_t *hmm = NULL;
	size_t index = hm->m_hash_func(ptr) % hm->m_capacity;
	
	if (!(hmm = (hash_bucket_mgr_member_t*)g_hm_alloc(sizeof(hash_bucket_mgr_member_t)))) {
		HASH_BUCKET_MGR_ERROR_LOG("g_hm_alloc error, errno: %d - %s", errno, strerror(errno));
		return false;
	}
	hmm->m_ptr = ptr;
	hmm->m_next = hm->m_member[index];
	if (hm->m_member[index]) hm->m_member[index]->m_prev = hmm;
	hm->m_member[index] = hmm;
	return true;
}

static hash_bucket_mgr_member_t* _hash_bucket_mgr_member_find(hash_bucket_mgr_t *hm, void *ptr)
{
	if (!hm || !ptr) return NULL;
	hash_bucket_mgr_member_t *hmm = NULL;
	size_t index = hm->m_hash_func(ptr) % hm->m_capacity;

	for (hmm = hm->m_member[index]; hmm; hmm = hmm->m_next) {
		if (hm->m_compare_func(ptr, hmm->m_ptr)) return hmm;
	}
	return NULL;
}

/* @func:
 *	添加节点到hash bucket中
 */
bool hash_bucket_mgr_member_add(hash_bucket_mgr_t *hm, void *ptr)
{
	if (!hm || !ptr) return false;
	bool ret = false;

	pthread_mutex_lock(&hm->m_mutex);
	ret = _hash_bucket_mgr_member_add(hm, ptr);
	pthread_mutex_unlock(&hm->m_mutex);
	
	return ret;
}

/* @func:
 *	从hash bucket中删除一个节点
 */
void* hash_bucket_mgr_member_del(hash_bucket_mgr_t *hm, void *ptr)
{
	if (!hm || !ptr) return false;
	void *ret = NULL;
	hash_bucket_mgr_member_t *hmm = NULL;
	size_t index = 0;
	
	pthread_mutex_lock(&hm->m_mutex);
	index = hm->m_hash_func(ptr) % hm->m_capacity;
	if ((hmm = _hash_bucket_mgr_member_find(hm, ptr))) {
		ret = hmm->m_ptr;
		if (!hmm->m_prev) {
			hm->m_member[index] = hmm->m_next;
			if (hmm->m_next) hmm->m_next->m_prev = NULL;
		} else if (!hmm->m_next) hmm->m_prev->m_next = NULL;
		else hmm->m_next->m_prev = hmm->m_prev, hmm->m_prev->m_next = hmm->m_next;
		g_hm_free(hmm);
	}
	pthread_mutex_unlock(&hm->m_mutex);
	
	return ret;
}

/* @func：
 *	判断一个节点是否存在
 */
bool hash_bucket_mgr_member_is_exist(hash_bucket_mgr_t *hm, void *ptr)
{
	if (!hm) return NULL;
	bool ret = false;
	pthread_mutex_lock(&hm->m_mutex);
	ret = _hash_bucket_mgr_member_find(hm, ptr) ? true : false;
	pthread_mutex_unlock(&hm->m_mutex);
	
	return ret;
}

#include <assert.h>
static size_t _hash_func(void *ptr)
{
	if (!ptr) return 0;
	return *(size_t*)ptr;
}

static bool _compare_func(void *src, void *dst)
{
	if (src == dst) return true;
	if (!src || !dst) return false;
	return (*(size_t*)src == *(size_t*)dst);
}

int main()
{
	size_t i = 0, j = 0, count = 1024, capacity = 50;
	size_t *num = NULL, num_2 = 0;
	pthread_t pt[20];
	hash_bucket_mgr_t *hm = NULL;
	
	hash_bucket_mgr_init(NULL, NULL);
	assert((hm = hash_bucket_mgr_new(capacity, _hash_func, _compare_func)));
	hash_bucket_mgr_dump(hm, true);
	
	/* 单线程测试 */
	for (i = 0; i < count; i++) {
		if (!(num = malloc(sizeof(int)))) continue;
		num_2 = *num = i % 3;
		assert(hash_bucket_mgr_member_add(hm, num));
		assert(hash_bucket_mgr_member_is_exist(hm, &num_2));
	}
	
	for (i = 0; i < count; i++) {
		num_2 = i % 3;
		assert((num = hash_bucket_mgr_member_del(hm, &num_2)));
		assert(*(size_t*)num == num_2);
		free(num);
	}
	hash_bucket_mgr_dump(hm, true);
	hash_bucket_mgr_free(hm);
	hm = NULL;
	MY_PRINTF("single thread ok");
	
	/* 多线程测试 */
	assert((hm = hash_bucket_mgr_new(capacity, _hash_func, _compare_func)));
	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
		pthread_create(&pt[i], NULL, ({
		void* _(void* arg) {
			size_t k = 0;
			size_t *num = NULL, num_2 = 0;
			for (k = 0; k < count; k++) {
				if (!(num = malloc(sizeof(int)))) continue;
				num_2 = *num = k;
				assert(hash_bucket_mgr_member_add(hm, num));
				assert(hash_bucket_mgr_member_is_exist(hm, &num_2));
				
				if (k % 5 == 0) {
					assert((num = hash_bucket_mgr_member_del(hm, &num_2)));
					assert(*(size_t*)num == num_2);
				}
			}
			return arg;
		}; _;}), NULL);
	}
	
	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
		pthread_join(pt[i], NULL);
	}
	
	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
		for (j = 0; j < count; j++) {
			if (j % 5 == 0) continue;
			num_2 = j;
			assert((num = hash_bucket_mgr_member_del(hm, &num_2)));
			assert(*(size_t*)num == num_2);
			free(num);
		}
	}
	hash_bucket_mgr_dump(hm, true);
	hash_bucket_mgr_free(hm);
	hm = NULL;
	
	MY_PRINTF("mutiple thread OK");
	return 0;
}