#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "linker_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define _TRACE_LOG MY_PRINTF
#define _DEBUG_LOG MY_PRINTF
#define _INFO_LOG MY_PRINTF
#define _WARN_LOG MY_PRINTF
#define _ERROR_LOG MY_PRINTF

typedef void* (*g_alloc_t) (size_t size);
typedef void (*g_free_t) (void *ptr);

static void* _malloc2calloc(size_t size);
static void _free(void *ptr);
static g_alloc_t g_alloc = _malloc2calloc;
static g_free_t g_free = _free;

static void* _malloc2calloc(size_t size)
{
    return calloc(1, size);

}

static void _free(void *ptr)
{
    if (!ptr) return ;
    free(ptr);
}

/* @func:
 *  获取一个管理器
 */
linker_mgr_t* linker_mgr_new(bool is_mutex, linker_free_cb free_cb)
{
    if (!free_cb) return NULL;
    linker_mgr_t *lm = NULL;

    if (lm = g_alloc(sizeof(*lm)), !lm) {
        _WARN_LOG("g_alloc error, errno: %d - %s", errno, strerror(errno));
        goto err;
    }

    if (is_mutex && pthread_rwlock_init(&lm->m_rwlock, NULL) != 0) {
        _WARN_LOG("pthead_rwlock_init error, errno: %d - %s", errno, strerror(errno));
        goto err;
    }

    lm->m_is_mutex = is_mutex;
    lm->m_free_cb = free_cb;
    return lm;

err:
    if (lm) g_free(lm);
    return NULL;
}

/* @func:
 *  销毁一个管理器
 */
void linker_mgr_free(linker_mgr_t *lm)
{
    if (!lm || !lm->m_free_cb) return ;
    linker_t *head = NULL, *next = NULL;
    
    if (lm->m_is_mutex) pthread_rwlock_wrlock(&lm->m_rwlock);
    for (head = lm->m_linker; head; head = next) {
        next = head->m_next;
        lm->m_free_cb(head->m_item);
        g_free(head);
    }
    if (lm->m_is_mutex) pthread_rwlock_destroy(&lm->m_rwlock);
    g_free(lm);
}

/* @func:
 *  添加一个节点
 */
bool linker_mgr_add(linker_mgr_t *lm, void *item)
{
    if (!lm || !item) return false;
    linker_t *linker = NULL;

    if (linker = g_alloc(sizeof(*linker)), !linker) {
        _WARN_LOG("g_alloc error, errno: %d - %s", errno, strerror(errno));
        return false;
    }

    linker->m_item = item;

    if (lm->m_is_mutex) pthread_rwlock_wrlock(&lm->m_rwlock);
    linker->m_next = lm->m_linker;
    lm->m_linker = linker;
    if (lm->m_is_mutex) pthread_rwlock_unlock(&lm->m_rwlock);
    return true;
}

/* @func:
 *  删除一个元素
 */
bool linker_mgr_del(linker_mgr_t *lm, void *sample, linker_equal_cb equal_cb)
{
    if (!lm || !sample || !equal_cb || !lm->m_free_cb) return false;
    linker_t *head = NULL, *prev = NULL;
    bool ret = false;

    if (lm->m_is_mutex) pthread_rwlock_wrlock(&lm->m_rwlock);
    for (head = lm->m_linker; head; head = head->m_next) {
        if (equal_cb(head->m_item, sample)) break;
        prev = head;
    }

    if (!head) goto out; /* 找不到 */

    if (!prev) lm->m_linker = head->m_next; /* 第一个 */
    else prev->m_next = head->m_next;

    lm->m_free_cb(head->m_item);
    g_free(head);
    ret = true;

out:
    if (lm->m_is_mutex) pthread_rwlock_unlock(&lm->m_rwlock);
    return ret;
}

static linker_t* _find(linker_mgr_t *lm, void *sample, linker_equal_cb equal_cb)
{
    if (!lm || !sample || !equal_cb) return NULL;
    linker_t *head = NULL;

    for (head = lm->m_linker; head; head = head->m_next) {
        if (equal_cb(head->m_item, sample)) return head;
    }
    return NULL;
}

/* @func:
 *  判读一个节点是否存在
 */
bool linker_mgr_is_exsit(linker_mgr_t *lm, void *sample, linker_equal_cb equal_cb)
{
    if (!lm || !sample || !equal_cb) return false;
    linker_t *linker = NULL;

    if (lm->m_is_mutex) pthread_rwlock_rdlock(&lm->m_rwlock);
    linker = _find(lm, sample, equal_cb);
    if (lm->m_is_mutex) pthread_rwlock_unlock(&lm->m_rwlock);
    return (linker ? true : false);
}

/* @func:
 *  遍历节点, 回调函数放回true,则不继续遍历
 */
bool linker_mgr_walk(linker_mgr_t *lm, linker_walk_cb walk_cb)
{
    if (!lm || !walk_cb) return false;
    linker_t *head = NULL;
    bool ret = false;

    if (lm->m_is_mutex) pthread_rwlock_rdlock(&lm->m_rwlock);
    for (head = lm->m_linker; head; head = head->m_next) {
        if (ret = walk_cb(head->m_item), ret) break;
    }
    if (lm->m_is_mutex) pthread_rwlock_unlock(&lm->m_rwlock);

    return ret;
}

/* @func:
 *  遍历节点,并可以改变节点的信息, 回调放回true则不继续遍历
 */
bool linker_mgr_walk_set(linker_mgr_t *lm, linker_walk_set_cb walk_set_cb, void *arg)
{
    if (!lm || !walk_set_cb) return false;
    linker_t *head = NULL;
    bool ret = false;

    if (lm->m_is_mutex) pthread_rwlock_wrlock(&lm->m_rwlock);
    for (head = lm->m_linker; head; head = head->m_next) {
        if (ret = walk_set_cb(head->m_item, arg), ret) break;
    }
    if (lm->m_is_mutex) pthread_rwlock_unlock(&lm->m_rwlock);
    return ret;
}


#if 1
#include <assert.h>

static void _free_cb(void *item)
{
    g_free(item);
}

static bool _equal_cb(void *first, void *second)
{
    if (!first || !second) return false;
    return *(int*)first == *(int*)second ? true : false;
}

static bool _walk_cb(void *item)
{
    if (!item) return false;
    MY_PRINTF("%d", *(int*)item);
    return false;
}

static bool _walk_set_cb(void *item, void *arg)
{
    if (!item || !arg) return false;
    if (*(int*)item != 2) return false;

    *(int*)item = *(int*)arg;
    return true;
}

int main()
{
    linker_mgr_t *lm = NULL;
    int *num1 = NULL, *num2 = NULL, *num3 = NULL;
    int cnt1 = 1, cnt2 = 2, cnt3 = 3, cnt4 = 4;

    assert((num1 = g_alloc(sizeof(*num1))));
    assert((num2 = g_alloc(sizeof(*num2))));
    assert((num3 = g_alloc(sizeof(*num3))));
    *num1 = cnt1, *num2 = cnt2, *num3 = cnt3;

    /* assert((lm = linker_mgr_new(false, _free_cb))); */
    assert((lm = linker_mgr_new(true, _free_cb)));
    assert(linker_mgr_add(lm, num1));
    assert(linker_mgr_add(lm, num2));
    assert(linker_mgr_add(lm, num3));

    assert(linker_mgr_is_exsit(lm, &cnt1, _equal_cb));
    assert(linker_mgr_is_exsit(lm, &cnt2, _equal_cb));
    assert(linker_mgr_is_exsit(lm, &cnt3, _equal_cb));
    assert(!linker_mgr_is_exsit(lm, &cnt4, _equal_cb));

    linker_mgr_walk(lm, _walk_cb);
    assert(linker_mgr_walk_set(lm, _walk_set_cb, &cnt4));
    assert(linker_mgr_is_exsit(lm, &cnt4, _equal_cb));
    linker_mgr_walk(lm, _walk_cb);

    assert(linker_mgr_del(lm, &cnt4, _equal_cb));
    assert(linker_mgr_del(lm, &cnt3, _equal_cb));
    assert(!linker_mgr_del(lm, &cnt2, _equal_cb));
    assert(linker_mgr_is_exsit(lm, &cnt1, _equal_cb));

    linker_mgr_free(lm);
    return 0;
}

#endif
