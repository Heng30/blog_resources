#ifndef _HASH_BUCKET_MGR_H_
#define _HASH_BUCKET_MGR_H_

#include <stdbool.h>

typedef void* (*hash_bucket_mgr_alloc_t) (size_t size);
typedef void (*hash_bucket_mgr_free_t) (void *ptr);
typedef size_t (*hash_bucket_hash_func_t) (void *ptr);
typedef bool (*hash_bucket_compare_func_t) (void *src, void *dst);

typedef struct _hash_bucket_mgr_member {
	void *m_ptr;
	struct _hash_bucket_mgr_member *m_prev;
	struct _hash_bucket_mgr_member *m_next;
} hash_bucket_mgr_member_t;

/* 全局节点管理器，一般用于套接字资源管理 */
typedef struct _hash_bucket_mgr {
	size_t m_capacity; /* hash管理器的大小 */
	pthread_mutex_t m_mutex;
	hash_bucket_mgr_member_t **m_member; /* hash节点数组 */
	hash_bucket_hash_func_t m_hash_func;
	hash_bucket_compare_func_t m_compare_func;
} hash_bucket_mgr_t;

/* @func:
 *	打印一个管理节点
 */
void hash_bucket_mgr_dump(hash_bucket_mgr_t *hm, bool is_dump_member);

/* @func:
 *	初始化管理器
 */
void hash_bucket_mgr_init(hash_bucket_mgr_alloc_t hash_bucket_alloc, hash_bucket_mgr_free_t hash_bucket_free);

/* @func:
 *	创建一个hash管理节点
 */
hash_bucket_mgr_t* hash_bucket_mgr_new(size_t capacity, hash_bucket_hash_func_t hash_func, 
								hash_bucket_compare_func_t compare_func);
								
/* @func:
 *	销毁一个管理节点，但不销毁真正ptr节点的内存
 */
void hash_bucket_mgr_free(hash_bucket_mgr_t *hm);

/* @func:
 *	添加节点到hash bucket中
 */
bool hash_bucket_mgr_member_add(hash_bucket_mgr_t *hm, void *ptr);

/* @func:
 *	从hash bucket中删除一个节点
 */
void* hash_bucket_mgr_member_del(hash_bucket_mgr_t *hm, void *ptr);

/* @func：
 *	判断一个节点是否存在
 */
bool hash_bucket_mgr_member_is_exist(hash_bucket_mgr_t *hm, void *ptr);

#endif