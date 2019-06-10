#ifndef _BUFFER_MGR_H_
#define _BUFFER_MGR_H_

#include <stdbool.h>

/* 所有的偏移都是相对于系统分配的内存起始位置 */
typedef struct _buffer_mgr {
	size_t m_start; /* 指向缓冲区开始的位置， */
	size_t m_end;	/* 指向缓冲区末尾的后一个位置 */
	size_t m_rd_offset; /* 可读取的偏移位置 */
	size_t m_wr_offset;	/* 可写入数据的偏移*/
	size_t m_reserved; /* 执行保留位置的偏移 */
	pthread_mutex_t m_mutex;
} buffer_mgr_t;

typedef void* (*buffer_mgr_alloc_t) (size_t size);
typedef void (*buffer_mgr_free_t) (void *ptr);

/* 缓冲区数据过滤函数，不修改缓冲区的内容 */
typedef void* (*buffer_mgr_filter_func_t)(const void *ptr, size_t len, void *arg);

/* 更新读取位置的函数，返回偏移位置 */
typedef size_t (*buffer_mgr_update_func_t) (const void *ptr, size_t len, void *arg);

void buffer_mgr_dump(buffer_mgr_t *bm);

/* @func:
 *	初始化管理器
 */
void buffer_mgr_init(buffer_mgr_alloc_t alloc, buffer_mgr_free_t dealloc);

/* @func:
 *	创建一个管理器
 */
buffer_mgr_t* buffer_mgr_new(size_t capacity, size_t reserved);

/* @func:
 *	销毁一个管理器
 */
void buffer_mgr_free(buffer_mgr_t *bm);


/* @func:
 *	从套接字中读取内容
 */
ssize_t buffer_mgr_read(buffer_mgr_t *bm, int fd);

/* @func:
 *	获取当前可读数据的副本
 */
void* buffer_mgr_copy_new(buffer_mgr_t *bm, size_t *len);

/* @func:
 *	释放副本占用的内存
 */
void buffer_mgr_copy_free(void *ptr);

/* @func:
 *	重置读写的偏移位置
 */
void buffer_mgr_reset(buffer_mgr_t *bm);


/* @func:
 *	将缓冲区的数据写入套接字
 */
ssize_t buffer_mgr_write(buffer_mgr_t *bm, int fd);

/* @func:
 *	对缓冲区的数据进行操作，
 * @warn:
 *	不能对缓冲区的数据进行修改, 因为修改后数据的大小可能发生变化，但管理节点的信息没有更新
 */
void* buffer_mgr_filter(buffer_mgr_t *bm, buffer_mgr_filter_func_t filter_func, void *arg);

/* @func:
 *	更新读取位置
 */
void buffer_mgr_update(buffer_mgr_t *bm, buffer_mgr_update_func_t update_func, void *arg);

/* @func:
 * 	设置reserved的内存
 */
bool buffer_mgr_reserved_set(buffer_mgr_t *bm, void *reserved, size_t len);

#endif
