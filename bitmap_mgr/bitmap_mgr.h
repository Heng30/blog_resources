#ifndef _bitmap_MGR_H_
#define _bitmap_MGR_H_

#include <stdbool.h>
#include <stdint.h>

typedef void* (*BITMAP_MGR_ALLOC_T) (size_t size);
typedef void (*BITMAP_MGR_FREE_T) (void *ptr);

typedef struct _bitmap_mgr {
    union {
        uint64_t m_size; /* 这个bitmap的大小 */
        struct _bitmap_mgr *m_next; /* 用于freelist */
    } u;
    uint8_t *m_map; /* bitmap */
    pthread_mutex_t m_mutex;
} bitmap_mgr_t;

/* @func:
 *  初始化参数
 * @warn:
 * 1.自定义的内存分配函数要保证初始化内存全为0
 * 2.使用系统的内存分配函数，alloc和dealloc参数应为NULL
 */
bool bitmap_mgr_init(BITMAP_MGR_ALLOC_T alloc, BITMAP_MGR_FREE_T dealloc);

/* @param:
 *  分配一个bitmap_mgr
 */
bitmap_mgr_t* bitmap_mgr_new(uint64_t size);

/* @func:
 *  释放bitmap占用的内存;
 *  将bitmap_mgr加入到freelist中
 */
bool bitmap_mgr_free(bitmap_mgr_t *bm);

/* @func:
 *       获取特定位的状态
 * @param:
 *      pos: 获取的位置，从0开始
 */
bool bitmap_mgr_status_get(bitmap_mgr_t *bm, uint64_t pos, uint8_t *status);

/* func:
 *      设定指定pos位
 */
bool bitmap_mgr_status_set(bitmap_mgr_t *bm, uint64_t pos, uint8_t status);

/* func:
 *      输出bitmap信息
 */
void bitmap_mgr_dump(bitmap_mgr_t *bm);

/* @func:
 *  销毁bitmap_mgr，释放内存
 * @warn:
 *  非线程安全
 */
bool bitmap_mgr_destroy(bitmap_mgr_t *bm);

/* @func:
 *  释放freelist的内存
 * @warn:
 *  1.
 *  使用者有义务保证放入freelist中的bitmap_mgr并没有被别的线程在使用，否则是非线程安全的
 */
void bitmap_mgr_gc(void);

#endif
