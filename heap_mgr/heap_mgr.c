#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

#include "heap_mgr.h"

#define MY_PRINT(format, ...) printf(format"\n", ##__VA_ARGS__)
#define HEAP_MGR_TRACE_LOG MY_PRINT
#define HEAP_MGR_DEBUG_LOG MY_PRINT
#define HEAP_MGR_INFO_LOG MY_PRINT
#define HEAP_MGR_WARN_LOG MY_PRINT
#define HEAP_MGR_ERROR_LOG MY_PRINT

#define HEAP_MEMBER_ROOT 0X1
#define HEAP_MEMBER_INIT_CAPACITY   0XF

#define HEAP_MGR_EXPAND_RATE 1.5f
#define HEAP_MGR_SHRINK_RATE 0.5f

#define HEAP_PARENT_2_LCHILD(pos) ((pos) << 0X1)
#define HEAP_PARENT_2_RCHILD(pos) (((pos) << 0X1) + 1)
#define HEAP_CHILD_2_PARENT(pos) ((pos) >> 0X1)
#define HEAP_INDEX_IS_VALID(hm, index) ((hm)->m_offset > (index) ? true : false)

static HEAP_MGR_ALLOC_T g_heap_mgr_alloc = NULL;
static HEAP_MGR_FREE_T g_heap_mgr_free = NULL;

static void* _malloc2calloc(size_t size)
{
    return calloc(1, size);
}

static bool _heap_mgr_expand(heap_mgr_t *hm)
{
    if (!hm || !hm->m_heap) return false;
    heap_mgr_member_t *hmm = NULL;
    size_t capacity = hm->m_capacity * HEAP_MGR_EXPAND_RATE + 1;

    if (!(hmm = g_heap_mgr_alloc(capacity * sizeof(heap_mgr_member_t)))) {
        HEAP_MGR_WARN_LOG("g_heap_mgr_alloc error, errno: %d - %s", errno, strerror(errno));
        return false;
    }
    memcpy(hmm, hm->m_heap, hm->m_capacity * sizeof(heap_mgr_member_t));
    g_heap_mgr_free(hm->m_heap);
    hm->m_capacity = capacity;
    hm->m_heap = hmm;
    return true;
}

static bool _heap_mgr_shrink(heap_mgr_t *hm)
{
    if (!hm || !hm->m_heap) return false;
    heap_mgr_member_t *hmm = NULL;
    size_t capacity = hm->m_capacity * HEAP_MGR_SHRINK_RATE;

    if ((hm->m_offset + HEAP_MEMBER_INIT_CAPACITY) >= capacity) return false;
    if (!(hmm = g_heap_mgr_alloc(capacity * sizeof(heap_mgr_member_t)))) {
        HEAP_MGR_WARN_LOG("g_heap_mgr_alloc error, errno: %d - %s", errno, strerror(errno));
        return false;
    }
    memcpy(hmm, hm->m_heap, (hm->m_offset + 1) * sizeof(heap_mgr_member_t));
    g_heap_mgr_free(hm->m_heap);
    hm->m_capacity = capacity;
    hm->m_heap = hmm;
    return true;
}

static bool _heap_mgr_member_filter_up(heap_mgr_t *hm)
{
    if (!hm || !hm->m_heap || !hm->m_compare) return false;
    if (hm->m_offset == HEAP_MEMBER_ROOT) return true;
    heap_mgr_member_t *hmm = hm->m_heap;
    size_t parent_index = 0, child_index = hm->m_offset;
    void *ptr = NULL;

    while (child_index > HEAP_MEMBER_ROOT) {
        parent_index = HEAP_CHILD_2_PARENT(child_index);
        if (!hm->m_compare(hmm[child_index].m_ptr, hmm[parent_index].m_ptr)) break;
        ptr = hmm[child_index].m_ptr;
        hmm[child_index].m_ptr = hmm[parent_index].m_ptr;
        hmm[parent_index].m_ptr = ptr;
        child_index = parent_index;
    }
    return true;
}


static bool _heap_mgr_member_filter_down(heap_mgr_t *hm)
{
    if (!hm || !hm->m_heap || !hm->m_compare) return false;
    if (hm->m_offset <= HEAP_MEMBER_ROOT) return false;
    heap_mgr_member_t *hmm = hm->m_heap;
    size_t parent_index = HEAP_MEMBER_ROOT, child_index = HEAP_MEMBER_ROOT, lchild_index = 0, rchild_index = 0;
    void *ptr = NULL;

    while (true) {
        lchild_index = HEAP_PARENT_2_LCHILD(parent_index);
        rchild_index = HEAP_PARENT_2_RCHILD(parent_index);
        if (HEAP_INDEX_IS_VALID(hm, lchild_index) && HEAP_INDEX_IS_VALID(hm, rchild_index)) {
            if (hm->m_compare(hmm[lchild_index].m_ptr, hmm[rchild_index].m_ptr)) child_index = lchild_index;
            else child_index = rchild_index;
        } else if (!HEAP_INDEX_IS_VALID(hm, lchild_index) && !HEAP_INDEX_IS_VALID(hm, rchild_index))  break;
        else child_index = lchild_index;

        if (!hm->m_compare(hmm[child_index].m_ptr, hmm[parent_index].m_ptr)) break;
        ptr = hmm[child_index].m_ptr;
        hmm[child_index].m_ptr = hmm[parent_index].m_ptr;
        hmm[parent_index].m_ptr = ptr;
        parent_index = child_index;
    }
    return true;
}

/* @func:
 *  初始化参数
 */
void heap_mgr_init(HEAP_MGR_ALLOC_T alloc, HEAP_MGR_FREE_T dealloc)
{
    if (!alloc || !dealloc) {
        g_heap_mgr_alloc = _malloc2calloc;
        g_heap_mgr_free = free;
    } else {
        g_heap_mgr_alloc = alloc;
        g_heap_mgr_free = dealloc;
    }
}

/* @param:
 *  分配一个heap_mgr
 */
heap_mgr_t* heap_mgr_new(size_t capacity, HEAP_MEMBER_COMPARE_T compare)
{
    if (!compare) return NULL;
    if (!capacity) capacity = HEAP_MEMBER_INIT_CAPACITY;
    heap_mgr_t *hm = NULL;

    if (!(hm = g_heap_mgr_alloc(sizeof(heap_mgr_t)))) {
        HEAP_MGR_ERROR_LOG("alloc error, errno: %d - %s", errno, strerror(errno));
        goto free_exit;
    }

    if (!(hm->m_heap = (heap_mgr_member_t*)g_heap_mgr_alloc(capacity * sizeof(heap_mgr_member_t)))) {
        HEAP_MGR_ERROR_LOG("calloc error, errno: %d-%s", errno, strerror(errno));
        goto free_exit;
	}

    if(pthread_mutex_init(&hm->m_mutex, NULL)) {
        HEAP_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
        goto free_exit;
    }

    hm->m_capacity = capacity;
    hm->m_compare = compare;
    hm->m_offset = 1;
	return hm;

free_exit:
    if (hm) {
        if (hm->m_heap) g_heap_mgr_free(hm->m_heap);
        g_heap_mgr_free(hm);
    }
    return NULL;
}

/* @func:
 *  销毁heap_mgr，释放内存
 * @warn:
 *  非线程安全
 */
bool heap_mgr_free(heap_mgr_t *hm)
{
    if (!hm) return false;

    pthread_mutex_lock(&hm->m_mutex);
    if (hm->m_heap) g_heap_mgr_free(hm->m_heap);
    hm->m_heap = NULL;
    pthread_mutex_destroy(&hm->m_mutex);
    g_heap_mgr_free(hm);

	return true;
}

/* @func:
 *  添加一个节点到heap
 */
bool heap_mgr_member_add(heap_mgr_t *hm, void *ptr)
{
	if(!hm) return false;
    bool ret = false;

    pthread_mutex_lock(&hm->m_mutex);
    if (hm->m_offset >= hm->m_capacity) {
        if (!_heap_mgr_expand(hm)) goto out;
    }

    hm->m_heap[hm->m_offset].m_ptr = ptr;
    _heap_mgr_member_filter_up(hm);
    hm->m_offset++;
    ret = true;

out:
    pthread_mutex_unlock(&hm->m_mutex);
    return ret;
}

/* func:
 *  删除一个节点
 */
void* heap_mgr_member_del(heap_mgr_t *hm)
{
	if(!hm) return NULL;
    void *ptr = NULL;

    pthread_mutex_lock(&hm->m_mutex);
    if (hm->m_offset > HEAP_MEMBER_ROOT) {
        ptr = hm->m_heap[HEAP_MEMBER_ROOT].m_ptr;
        hm->m_offset--;
        hm->m_heap[HEAP_MEMBER_ROOT].m_ptr = hm->m_heap[hm->m_offset].m_ptr;
        _heap_mgr_member_filter_down(hm);
        _heap_mgr_shrink(hm);
    }
    pthread_mutex_unlock(&hm->m_mutex);

	return ptr;
}

/* @func:
 *  判断heap是否为空
 */
bool heap_mgr_is_empty(heap_mgr_t *hm)
{
    if (!hm) return true;
    bool ret = false;

    pthread_mutex_lock(&hm->m_mutex);
    if (!hm->m_heap || hm->m_offset == 1) ret = true;
    pthread_mutex_unlock(&hm->m_mutex);
    return ret;
}

/* func:
 *      输出heap信息
 */
void heap_mgr_dump(heap_mgr_t *hm)
{
    if (!hm) return ;
    size_t i = 0;
    heap_mgr_member_t *hmm = NULL;

    HEAP_MGR_TRACE_LOG("===========");
    pthread_mutex_lock(&hm->m_mutex);
    HEAP_MGR_TRACE_LOG("capacity: %lu", hm->m_capacity);
    HEAP_MGR_TRACE_LOG("offset: %lu", hm->m_offset);
    HEAP_MGR_TRACE_LOG("compare: %p", hm->m_compare);
    HEAP_MGR_TRACE_LOG("heap: %p", hm->m_heap);
    HEAP_MGR_TRACE_LOG();
    for (i = HEAP_MEMBER_ROOT; i < hm->m_offset; i++) {
        hmm = &hm->m_heap[i];
        HEAP_MGR_TRACE_LOG("index: %lu", i);
        HEAP_MGR_TRACE_LOG("ptr: %p", hmm->m_ptr);
        /* HEAP_MGR_TRACE_LOG("ptr_value: %d", *(int*)hmm->m_ptr); */
        HEAP_MGR_TRACE_LOG();
    }
    pthread_mutex_unlock(&hm->m_mutex);
    HEAP_MGR_TRACE_LOG("===========");
}

static bool _heap_mgr_test_compare(void *src, void *dst)
{
    if (!src || !dst) return false;
    return *(int*)src > *(int*)dst ? true : false;
}

/* @func:
 *  单线程测试，测试正确性
 */
#include <assert.h>
static void _heap_mgr_test_single(void)
{
    #define member_size 10
    int a_1 = 3, a_2 = 4, a_3 = 1, a_4 = 6, a_5 = 5, a_6 = 2;
    void *ptr = NULL;
    heap_mgr_t *hm = NULL;
    heap_mgr_init(NULL, NULL);
    assert((hm = heap_mgr_new(10, _heap_mgr_test_compare)));
    heap_mgr_dump(hm);
    assert(heap_mgr_is_empty(hm));
    assert(!(ptr = heap_mgr_member_del(hm)));

    assert(heap_mgr_member_add(hm, &a_1));
    assert(heap_mgr_member_add(hm, &a_2));
    assert(heap_mgr_member_add(hm, &a_3));
    assert(heap_mgr_member_add(hm, &a_4));
    assert(heap_mgr_member_add(hm, &a_5));
    assert(heap_mgr_member_add(hm, &a_6));
    assert(!heap_mgr_is_empty(hm));

    assert((ptr = heap_mgr_member_del(hm)));
    assert(ptr == &a_4);
    assert((ptr = heap_mgr_member_del(hm)));
    assert(ptr == &a_5);
    assert((ptr = heap_mgr_member_del(hm)));
    assert(ptr == &a_2);
    assert((ptr = heap_mgr_member_del(hm)));
    assert(ptr == &a_1);
    assert((ptr = heap_mgr_member_del(hm)));
    assert(ptr == &a_6);
    assert((ptr = heap_mgr_member_del(hm)));
    assert(ptr == &a_3);
    assert(heap_mgr_is_empty(hm));

    heap_mgr_dump(hm);
    heap_mgr_free(hm);

    MY_PRINT("single thread test ok");
}

/* @func:
 *  多线程测试，测试不会出现死锁
 */
static void _heap_mgr_test_multiple(void)
{
    #define pt_count 20
    int i = 0;
    size_t j = 0, offset = 0;
    int max_num = 0;

    void *ptr = NULL;
    pthread_t pt[pt_count];
    heap_mgr_t *hm = NULL;
    heap_mgr_init(NULL, NULL);
    assert((hm = heap_mgr_new(6, _heap_mgr_test_compare)));

    for (i = 0; i < pt_count; i++) {
        pthread_create(&pt[i], NULL, ({ 
            void* _(void *arg) {
                int j = 0, count = 1024 * 100;
                int *num = NULL;
                for (j = 0; j < count; j++) {
                    if (!(num = (int*)malloc(sizeof(int) * 100))) continue;
                    *num = j;
                    assert(heap_mgr_member_add(hm, num));
                    if (j % 2 == 0) heap_mgr_member_del(hm);
                }
                return arg;
            }; _; }), NULL);
    }

    for (i = 0; i < pt_count; i++) {
        pthread_join(pt[i], NULL);
    }

    ptr = heap_mgr_member_del(hm);
    max_num = *(int*)ptr;
    offset = hm->m_offset;
    for (j = 1; j < offset; j++) {
        ptr = heap_mgr_member_del(hm);
        assert(max_num >= *(int*)ptr);
        max_num = *(int*)ptr;
    }

    assert(heap_mgr_is_empty(hm));
    heap_mgr_dump(hm);
    heap_mgr_free(hm);
    MY_PRINT("multiple threads ok");
}

int main(void)
{
    _heap_mgr_test_single();
    _heap_mgr_test_multiple();

    return 0;
}
