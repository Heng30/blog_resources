#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "bitmap_mgr.h"

#define MY_PRINT(format, ...) printf(format"\n", ##__VA_ARGS__)
#define BITMAP_MGR_TRACE_LOG MY_PRINT
#define BITMAP_MGR_DEBUG_LOG MY_PRINT
#define BITMAP_MGR_INFO_LOG MY_PRINT
#define BITMAP_MGR_WARN_LOG MY_PRINT
#define BITMAP_MGR_ERROR_LOG MY_PRINT

#define BITMAP_POS_2_INDEX(pos) ((pos) >> 0x3) /* 查找pos的下标 */
#define BITMAP_POS_2_OFFSET(pos) ((pos) & 0x7) /* 查找pos在8bit中的偏移 */
#define BITMAP_POS_2_MASK(pos) (0x1 << BITMAP_POS_2_OFFSET(pos)) /* 获取pos在8bit中的掩码 */
#define m_size u.m_size
#define m_next u.m_next

static BITMAP_MGR_ALLOC_T g_bitmap_mgr_alloc = NULL;
static BITMAP_MGR_FREE_T g_bitmap_mgr_free = NULL;
static bitmap_mgr_t *g_freelist = NULL;
static pthread_mutex_t g_mutex;


static void* _malloc2calloc(size_t size)
{
    return calloc(1, size);
}

/* @func:
 *  添加到freelist中
 */
static void _bitmap_mgr_freelist_push(bitmap_mgr_t *bm) {
    if (!bm) return ;
    pthread_mutex_lock(&g_mutex);
    if (g_freelist) bm->m_next = g_freelist;
    g_freelist = bm;
    pthread_mutex_unlock(&g_mutex);
}

/* @func:
 *  从freelist中获取一个可用的节点
 */
static bitmap_mgr_t* _bitmap_mgr_freelist_pop(void)
{
    bitmap_mgr_t *bm = NULL;
    pthread_mutex_lock(&g_mutex);
    if (g_freelist) {
        bm = g_freelist;
        g_freelist = g_freelist->m_next;
    }
    pthread_mutex_unlock(&g_mutex);
    return bm;
}

/* @func:
 *  初始化参数
 */
bool bitmap_mgr_init(BITMAP_MGR_ALLOC_T alloc, BITMAP_MGR_FREE_T dealloc)
{
    if (pthread_mutex_init(&g_mutex, NULL)) {
        BITMAP_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
        return false;
    }

    if (!alloc || !dealloc) {
        g_bitmap_mgr_alloc = _malloc2calloc;
        g_bitmap_mgr_free = free;
    } else {
        g_bitmap_mgr_alloc = alloc;
        g_bitmap_mgr_free = dealloc;
    }
    return true;
}

/* @param:
 *  分配一个bitmap_mgr
 */
bitmap_mgr_t* bitmap_mgr_new(uint64_t size)
{
    if (!size) return NULL;
    bitmap_mgr_t *bm = NULL;
    bool is_freelist = false;
    uint64_t align = (size + 8 * sizeof(uint8_t) - 1) >> 0x3;

    if ((bm = _bitmap_mgr_freelist_pop())) is_freelist = true;
    else {
       if (!(bm = g_bitmap_mgr_alloc(sizeof(bitmap_mgr_t)))) {
           BITMAP_MGR_ERROR_LOG("alloc error, errno: %d - %s", errno, strerror(errno));
           goto free_exit;
       }
    }

    if (!(bm->m_map = (uint8_t*)g_bitmap_mgr_alloc(sizeof(uint8_t) * align))) {
        BITMAP_MGR_ERROR_LOG("calloc error, errno: %d-%s", errno, strerror(errno));
        goto free_exit;
	}

    if(!is_freelist && pthread_mutex_init(&bm->m_mutex, NULL)) {
        BITMAP_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
        goto free_exit;
    }

    bm->m_size = size;
	return bm;

free_exit:
    if (bm && !is_freelist) {
        if (bm->m_map) g_bitmap_mgr_free(bm->m_map);
        g_bitmap_mgr_free(bm);
    }
    return NULL;
}

/* @func:
 *  释放bitmap占用的内存
 */
bool bitmap_mgr_free(bitmap_mgr_t *bm)
{
    if (!bm) return false;

    pthread_mutex_lock(&bm->m_mutex);
    if (bm->m_map) g_bitmap_mgr_free(bm->m_map);
    bm->m_map = NULL;
    memset(&bm->u, 0, sizeof(bm->u));
    pthread_mutex_unlock(&bm->m_mutex);

    _bitmap_mgr_freelist_push(bm);
	return true;
}

/* @func:
 *  销毁bitmap_mgr，释放内存
 * @warn:
 *  非线程安全
 */
bool bitmap_mgr_destroy(bitmap_mgr_t *bm)
{
    if (!bm) return false;
    bitmap_mgr_free(bm);
    pthread_mutex_destroy(&bm->m_mutex);
    g_bitmap_mgr_free(bm);
    return true;
}

/* @func:
 *  释放freelist的内存
 * @warn:
 *  要保证在freelist中的bitmap_mgr没有在使用
 */
void bitmap_mgr_gc(void)
{
    bitmap_mgr_t *bm = NULL;
    pthread_mutex_lock(&g_mutex);
    for ( ; g_freelist; g_freelist = bm) {
        bm = g_freelist->m_next;
        pthread_mutex_destroy(&g_freelist->m_mutex);
        g_bitmap_mgr_free(g_freelist);
    }
    pthread_mutex_unlock(&g_mutex);
}

/* @func:
 *       获取特定位的状态
 * @param:
 *      pos: 获取的位置，从0开始
 */
bool bitmap_mgr_status_get(bitmap_mgr_t *bm, uint64_t pos, uint8_t *status)
{
	if(!bm || !status) return false;
    uint32_t index = 0, offset = 0;
    bool ret = false;

    pthread_mutex_lock(&bm->m_mutex);
    if (bm->m_map && pos < bm->m_size) {
        index = BITMAP_POS_2_INDEX(pos);
        offset = BITMAP_POS_2_OFFSET(pos);
        *status = (bm->m_map[index] >> offset) & 0X1;
        ret = true;
    }
    pthread_mutex_unlock(&bm->m_mutex);

    return ret;
}

/* func:
 *      设定指定pos位
 */
bool bitmap_mgr_status_set(bitmap_mgr_t *bm, uint64_t pos, uint8_t status)
{
	if(!bm) return false;
    uint32_t index = BITMAP_POS_2_INDEX(pos);
    bool ret = false;
    status &= 0X1;

    pthread_mutex_lock(&bm->m_mutex);
    if (bm->m_map && pos < bm->m_size) {
        if (0X1 == status) bm->m_map[index] |= BITMAP_POS_2_MASK(pos);
        else bm->m_map[index] &= ~BITMAP_POS_2_MASK(pos);
        ret = true;
    }
    pthread_mutex_unlock(&bm->m_mutex);

	return ret;
}

/* @func:
 *  打印bitmap
 */
static void _bitmap_mgr_map_dump(uint8_t *map, size_t size)
{
    if (!map) return ;
    uint64_t i = 0;
    char buf[sizeof(uint8_t) * 8 * 2] = {0};
    uint8_t ch = 0;
    uint64_t align = (size + 8 * sizeof(uint8_t) - 1) >> 0X3;

    for (i = 0; i < align; i++) {
        ch = map[i];
        snprintf(buf, sizeof(buf), "%d-%d-%d-%d-%d-%d-%d-%d", (ch >> 0X7) & 0X1, (ch >> 0X6) & 0X1,
                (ch >> 0X5) & 0X1, (ch >> 0X4) & 0X1, (ch >> 0X3) & 0X1, (ch >> 0X2) & 0X1,
                (ch >> 0X1) & 0X1, ch & 0X1);
        BITMAP_MGR_TRACE_LOG("Index: %lu, %s", i, buf);
    }
}

/* func:
 *      输出bitmap信息
 */
void bitmap_mgr_dump(bitmap_mgr_t *bm)
{
    if (!bm) return ;

    BITMAP_MGR_TRACE_LOG("===========");
    pthread_mutex_lock(&bm->m_mutex);
    BITMAP_MGR_TRACE_LOG("size: %lu", bm->m_size);
    _bitmap_mgr_map_dump(bm->m_map, bm->m_size);
    pthread_mutex_unlock(&bm->m_mutex);
    BITMAP_MGR_TRACE_LOG("===========");
}


/* @func:
 *  单线程测试，测试正确性
 */
static void _bitmap_mgr_test_single(void)
{
    #define member_size 10
    uint64_t i = 0, j = 0;
    uint64_t size[member_size] = {0};
    uint8_t status = 0;
    bitmap_mgr_t *bm[member_size] = {NULL};

    bitmap_mgr_init(NULL, NULL);
    for (i = 0; i < member_size; i++) {
        size[i] =  7 + i * j;
        assert((bm[i] = bitmap_mgr_new(size[i])));
        assert(bm[i]->m_size == size[i]);

        for (j = 0; j < size[i]; j++) {
            assert(bitmap_mgr_status_get(bm[i], j, &status));
            assert(status == 0);
            assert(bitmap_mgr_status_set(bm[i], j, 1));
            assert(bitmap_mgr_status_get(bm[i], j, &status));
            assert(status == 1);
        }

        bitmap_mgr_free(bm[i]);
    }
    MY_PRINT("string thread test ok");
}

/* @func:
 *  多线程测试，测试不会出现死锁
 */
static void _bitmap_mgr_test_multiple(void)
{
    #define pt_count 10
    int i = 0;
    uint64_t size = 102;
    pthread_t pt[pt_count];
    bitmap_mgr_t *bm = NULL;

    bitmap_mgr_init(NULL, NULL);
    assert((bm = bitmap_mgr_new(size)));
    assert(bm->m_size == size);

    for (i = 0; i < pt_count; i++) {
        assert(!pthread_create(&pt[i], NULL, ({
            void* _(void *arg) {
                assert(arg);
                bitmap_mgr_t *bm = (bitmap_mgr_t*)arg;
                uint8_t status = 0;
                uint64_t i = 0;

                for (i = 0; i < bm->m_size; i++) {
                    assert(bitmap_mgr_status_get(bm, i, &status));
                    assert(bitmap_mgr_status_set(bm, i, i % 2));
                }
                return NULL;
            }; _; }), bm));
    }

    for (i = 0; i < pt_count; i++) {
        pthread_join(pt[i], NULL);
    }

    bitmap_mgr_dump(bm);
    bitmap_mgr_destroy(bm);
    MY_PRINT("multiple threads ok");
}

/* @func:
 *  分配策略是线程安全的
 */
static void _bitmap_mgr_test_multiple_2(void)
{
    #define pt_count 10
    int i = 0;
    uint64_t size = 102;
    pthread_t pt[pt_count];
    bitmap_mgr_t *bm = NULL;

    bitmap_mgr_init(NULL, NULL);

    for (i = 0; i < pt_count; i++) {
        assert(!pthread_create(&pt[i], NULL, ({
            void* _(void *arg) {
                bitmap_mgr_t *bm = NULL;
                uint8_t status = 0;
                uint64_t i = 0, count = 1000;
                for (i = 0; i < count; i++) {
                    if (!(bm = bitmap_mgr_new(size))) {
                        MY_PRINT("new error");
                        continue;
                    }
                    bitmap_mgr_status_get(bm, i, &status);
                    bitmap_mgr_status_set(bm, i, i % 2);
                    bitmap_mgr_free(bm);

                    if (i % (count / 10) == 0) bitmap_mgr_gc();
                }
                return NULL;
            }; _; }), bm));
    }

    for (i = 0; i < pt_count; i++) {
        pthread_join(pt[i], NULL);
    }

    bitmap_mgr_gc();
    MY_PRINT("multiple threads_2 ok");
}

int main(void)
{
    _bitmap_mgr_test_single();
    _bitmap_mgr_test_multiple();
    _bitmap_mgr_test_multiple_2();

    return 0;
}

#undef m_size
#undef m_next
