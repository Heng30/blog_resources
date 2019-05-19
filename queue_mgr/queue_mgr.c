#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "queue_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define QUEUE_MGR_TRACE_LOG MY_PRINTF
#define QUEUE_MGR_DEBUG_LOG MY_PRINTF
#define QUEUE_MGR_INFO_LOG MY_PRINTF
#define QUEUE_MGR_WARN_LOG MY_PRINTF
#define QUEUE_MGR_ERROR_LOG MY_PRINTF

static queue_mgr_alloc_t g_qm_alloc = NULL;
static queue_mgr_free_t g_qm_free = NULL;

static void* _malloc2alloc(size_t size)
{
    return calloc(1, size);
}

/* @func:
 *  初始化队列
 */
void queue_mgr_init(queue_mgr_alloc_t alloc, queue_mgr_free_t decalloc)
{
    if (!alloc || !decalloc) g_qm_alloc = _malloc2alloc, g_qm_free = free;
    else g_qm_alloc = alloc, g_qm_free = decalloc;
}

/* @func:
 * 创建一个队列
 */
queue_mgr_t* queue_mgr_new(void)
{
    if (!g_qm_alloc || !g_qm_free) return NULL;
    queue_mgr_t *qm = NULL;

    if (!(qm = (queue_mgr_t*)g_qm_alloc(sizeof(queue_mgr_t)))) {
        QUEUE_MGR_ERROR_LOG("g_qm_alloc error, errno: %d - %s", errno, strerror(errno));
        return NULL;
    }

    if (pthread_mutex_init(&qm->m_mutex, NULL)) {
        QUEUE_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
        goto err;
    }

    return qm;

err:
    if (qm) g_qm_free(qm);
    return NULL;
}

/* @func:
 *  销毁一个队列
 */

bool queue_mgr_free(queue_mgr_t *qm, queue_mgr_ptr_free_t dealloc)
{
    if (!qm || !dealloc) return false;
    queue_mgr_member_t *qmm_cur = NULL, *qmm_next = NULL;

    pthread_mutex_lock(&qm->m_mutex);
    for (qmm_cur = qm->m_member; qmm_cur && qmm_next != qm->m_member; qmm_cur = qmm_next) {
        qmm_next = qmm_cur->m_next;
        if (qmm_cur->m_ptr) dealloc(qmm_cur->m_ptr);
    }
    pthread_mutex_destroy(&qm->m_mutex);
    g_qm_free(qm);
    g_qm_alloc = NULL;
    g_qm_free = NULL;
    return true;
}

/* @func:
 *  入队列头
 */
bool queue_mgr_push_front(queue_mgr_t *qm, void* ptr)
{
	if(!qm || !ptr) return false;
    queue_mgr_member_t *qmm = NULL;

    if (!(qmm = (queue_mgr_member_t*)g_qm_alloc(sizeof(queue_mgr_member_t)))) {
        QUEUE_MGR_WARN_LOG("g_qm_alloc error, errno: %d - %s", errno, strerror(errno));
        return false;
    }

    pthread_mutex_lock(&qm->m_mutex);
    qmm->m_ptr = ptr;
    if (!qm->m_member) qmm->m_prev = qmm->m_next = qmm, qm->m_member = qmm;
    else {
        qmm->m_prev = qm->m_member->m_prev, qmm->m_next = qm->m_member;
        qm->m_member->m_prev->m_next = qmm, qm->m_member->m_prev = qmm;
        qm->m_member = qmm;
    }

    pthread_mutex_unlock(&qm->m_mutex);

    return true;
}

/* @func:
 *  入队列尾
 */
bool queue_mgr_push_back(queue_mgr_t *qm, void* ptr)
{
	if(!qm || !ptr) return false;
    queue_mgr_member_t *qmm = NULL;

    if (!(qmm = (queue_mgr_member_t*)g_qm_alloc(sizeof(queue_mgr_member_t)))) {
        QUEUE_MGR_WARN_LOG("g_qm_alloc error, errno: %d - %s", errno, strerror(errno));
        return false;
    }

    pthread_mutex_lock(&qm->m_mutex);
    qmm->m_ptr = ptr;
    if (!qm->m_member) qmm->m_prev = qmm->m_next = qmm, qm->m_member = qmm;
    else {
        qmm->m_prev = qm->m_member->m_prev, qmm->m_next = qm->m_member;
        qm->m_member->m_prev->m_next = qmm, qm->m_member->m_prev = qmm;
    }
    pthread_mutex_unlock(&qm->m_mutex);

    return true;
}

/* @func:
 * 出队列头
 */
void* queue_mgr_pop_front(queue_mgr_t *qm)
{
	if (!qm) return NULL;
    queue_mgr_member_t *qmm = NULL;
    void *ptr = NULL;

    pthread_mutex_lock(&qm->m_mutex);
    if (!qm->m_member) ptr = NULL;
    else {
        qmm = qm->m_member;
        if (qmm->m_prev == qm->m_member || qmm->m_next == qm->m_member) {
            ptr = qmm->m_ptr, qm->m_member = NULL;
        } else {
            qmm->m_next->m_prev = qmm->m_prev;
            qmm->m_prev->m_next = qmm->m_next;
            qm->m_member = qmm->m_next;
            ptr = qmm->m_ptr;
        }
        g_qm_free(qmm);
    }
    pthread_mutex_unlock(&qm->m_mutex);

    return ptr;
}

/* @func:
 * 出队列尾
 */
void* queue_mgr_pop_back(queue_mgr_t *qm)
{
	if (!qm) return NULL;
    queue_mgr_member_t *qmm = NULL;
    void *ptr = NULL;

    pthread_mutex_lock(&qm->m_mutex);
    if (!qm->m_member) ptr = NULL;
    else {
        if (qm->m_member->m_prev == qm->m_member || qm->m_member->m_next == qm->m_member) {
            qmm = qm->m_member;
            ptr = qm->m_member->m_ptr;
            qm->m_member = NULL;
        } else {
            qmm = qm->m_member->m_prev;
            ptr = qmm->m_ptr;
            qmm->m_prev->m_next = qm->m_member;
            qm->m_member->m_prev = qmm->m_prev;
        }
        g_qm_free(qmm);
    }
    pthread_mutex_unlock(&qm->m_mutex);

    return ptr;
}

/* @func:
 *  是否为空
 */
bool queue_mgr_is_empty(queue_mgr_t *qm)
{
    if (!qm) return true;
    bool ret = false;

    pthread_mutex_lock(&qm->m_mutex);
    ret = qm->m_member ? false : true;
    pthread_mutex_unlock(&qm->m_mutex);

    return ret;
}

/* @func:
 *  打印信息
 */
void queue_mgr_dump(queue_mgr_t *qm)
{
    if (!qm) return ;
    queue_mgr_member_t *qmm_cur = NULL, *qmm_next = NULL;

    pthread_mutex_lock(&qm->m_mutex);
    for (qmm_cur = qm->m_member; qmm_cur && qmm_next != qm->m_member; qmm_cur = qmm_next) {
        qmm_next = qmm_cur->m_next;
        QUEUE_MGR_TRACE_LOG("=========");
        QUEUE_MGR_TRACE_LOG("ptr: %p", qmm_cur->m_ptr);
        QUEUE_MGR_TRACE_LOG("prev: %p", qmm_cur->m_prev);
        QUEUE_MGR_TRACE_LOG("self: %p", qmm_cur);
        QUEUE_MGR_TRACE_LOG("next: %p", qmm_cur->m_next);
        QUEUE_MGR_TRACE_LOG("=========");
    }
    pthread_mutex_unlock(&qm->m_mutex);
}

int main()
{
#include <assert.h>

    char *ptr = NULL;
    char *ptr_1 = (char*)malloc(10);
    char *ptr_2 = (char*)malloc(10);
    char *ptr_3 = (char*)malloc(10);
    char *ptr_4 = (char*)malloc(10);
    queue_mgr_t *qm = NULL;

    queue_mgr_init(NULL, NULL);
    assert((qm = queue_mgr_new()));
    assert(queue_mgr_is_empty(qm));
    assert(!queue_mgr_pop_back(qm));
    assert(!queue_mgr_pop_front(qm));
    assert(queue_mgr_push_front(qm, ptr_1));
    assert(queue_mgr_push_back(qm, ptr_2));
    assert(queue_mgr_push_back(qm, ptr_3));
    assert(queue_mgr_push_front(qm, ptr_4));
    queue_mgr_dump(qm);

    assert((ptr_3 == (ptr = queue_mgr_pop_back(qm))));
    free(ptr);
    assert((ptr_4 == (ptr = queue_mgr_pop_front(qm))));
    free(ptr);
    queue_mgr_dump(qm);
    assert(!queue_mgr_is_empty(qm));
    assert(queue_mgr_free(qm, free));

    /* 多线程测试 */
    queue_mgr_init(NULL, NULL);
    assert((qm = queue_mgr_new()));
    pthread_t pt[30];
    size_t i = 0;
    for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
        pthread_create(&pt[i], NULL, ({
                    void* _(void *arg) {
                    char *ptr = NULL;
                    size_t j = 0;
                    for (j = 0; j < 1024 * 1024; j++) {
                    ptr = (char*)malloc(10);
                    assert(queue_mgr_push_front(qm, ptr));
                    ptr = (char*)malloc(10);
                    assert(queue_mgr_push_back(qm, ptr));
                    if (j % 2 == 0){
                    ptr = (queue_mgr_pop_back(qm));
                    if (ptr) free(ptr);
                    } else if (j % 3 == 0) {
                    ptr = (queue_mgr_pop_front(qm));
                    if (ptr) free(ptr);
                    }
                    }
                    return arg;
                    }; _;}), NULL);

    }

    for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
        pthread_join(pt[i], NULL);
    }
    assert(queue_mgr_free(qm, free));
    MY_PRINTF("ok");
    return 0;
}
