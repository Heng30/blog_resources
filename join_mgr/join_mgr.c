#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "join_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define JOIN_MGR_TRACE_LOG MY_PRINTF
#define JOIN_MGR_DEBUG_LOG MY_PRINTF
#define JOIN_MGR_INFO_LOG MY_PRINTF
#define JOIN_MGR_WARN_LOG MY_PRINTF
#define JOIN_MGR_ERROR_LOG MY_PRINTF

static join_mgr_alloc_t g_jm_alloc = NULL;
static join_mgr_free_t g_jm_free = NULL;

static void* _malloc2calloc(size_t size)
{
    return calloc(1, size);
}

/* @func:
 *  打印信息
 */
void join_mgr_dump(join_mgr_t *jm, bool is_dump_body)
{
    if (!jm) return ;
    join_mgr_body_t *jmb = NULL;

    pthread_mutex_lock(&jm->m_mutex);
    JOIN_MGR_TRACE_LOG("==========");
    JOIN_MGR_TRACE_LOG("head: %p", jm->m_head);
    JOIN_MGR_TRACE_LOG("tail: %p", jm->m_tail);
    JOIN_MGR_TRACE_LOG("separator: %p", jm->m_separator);
    JOIN_MGR_TRACE_LOG("head_len: %lu", jm->m_head_len);
    JOIN_MGR_TRACE_LOG("tail_len: %lu", jm->m_tail_len);
    JOIN_MGR_TRACE_LOG("separator_len: %lu", jm->m_separator_len);
    JOIN_MGR_TRACE_LOG("part_free: %p", jm->m_part_free);
    JOIN_MGR_TRACE_LOG("body.m_first: %p", jm->m_body.m_first);
    JOIN_MGR_TRACE_LOG("body.m_last: %p", jm->m_body.m_last);

    if (is_dump_body) {
        for (jmb = jm->m_body.m_first; jmb; jmb = jmb->m_next) {
            JOIN_MGR_TRACE_LOG("+++++");
            JOIN_MGR_TRACE_LOG("len: %lu", jmb->m_len);
            JOIN_MGR_TRACE_LOG("part: %p", jmb->m_part);
            JOIN_MGR_TRACE_LOG("self: %p", jmb);
            JOIN_MGR_TRACE_LOG("next: %p", jmb->m_next);
            JOIN_MGR_TRACE_LOG("+++++");
        }
    }
    JOIN_MGR_TRACE_LOG("==========");
    pthread_mutex_unlock(&jm->m_mutex);
}

/* @func:
 *  初始化信息
 */
void join_mgr_init(join_mgr_alloc_t alloc, join_mgr_free_t dealloc)
{
    if (!alloc || !dealloc) g_jm_alloc = _malloc2calloc, g_jm_free = free;
    else g_jm_alloc = alloc, g_jm_free = dealloc;
}

/* @func:
 *  创建一个管理节点
 */
join_mgr_t* join_mgr_new(const void *head, size_t head_len, const void *tail, size_t tail_len,
                            const void *separator, size_t separator_len, join_mgr_part_free_t part_free)
{
    if (!part_free) part_free = free;
    join_mgr_t *jm = NULL;

    if (!(jm = g_jm_alloc(sizeof(join_mgr_t)))) {
        JOIN_MGR_ERROR_LOG("g_jm_alloc error, errno: %d - %s", errno, strerror(errno));
        return NULL;
    }

    if (pthread_mutex_init(&jm->m_mutex, NULL)) {
        JOIN_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
        goto err;
    }

    jm->m_head = head, jm->m_head_len = head_len;
    jm->m_tail = tail, jm->m_tail_len = tail_len;
    jm->m_separator = separator, jm->m_separator_len = separator_len;
    jm->m_part_free = part_free;

    return jm;

err:
    if (jm) free(jm);
    return NULL;
}

/* @func:
 *  添加一个节点到管理器
 */
bool join_mgr_add(join_mgr_t *jm, const void *part, size_t part_len)
{
    if (!jm || !part || !part_len) return false;
    join_mgr_body_t *jmb = NULL;

    if (!(jmb = g_jm_alloc(sizeof(join_mgr_body_t)))) {
        JOIN_MGR_WARN_LOG("g_jm_alloc error, errno: %d - %s", errno, strerror(errno));
        return false;
    }

    jmb->m_part = part, jmb->m_len = part_len;

    pthread_mutex_lock(&jm->m_mutex);
    if (!jm->m_body.m_first || !jm->m_body.m_last) jm->m_body.m_first = jm->m_body.m_last = jmb;
    else jm->m_body.m_last->m_next = jmb, jm->m_body.m_last = jmb;
    pthread_mutex_unlock(&jm->m_mutex);

    return true;
}


/* @func:
 *  拼接内存
 *  head+(part1+separator+part2+separator+...+parnN)+tail
 * @return:
 *  true: 还有内存没有拼接
 *  false: 内存都拼接完
 */
bool join_mgr_join(join_mgr_t *jm, void *buf, size_t buf_size, size_t *read_len)
{
    if (!jm || !buf || !buf_size || !read_len) return false;
    join_mgr_body_t *jmb = NULL, *jmb_first = NULL, *jmb_last = NULL;
    *read_len = 0;
    memset(buf, 0, 1);
    size_t total_len = 0;
    bool ret = false, is_join_body = false;
    join_mgr_t jm_tmp = *jm;

    /* 获取缓冲区能够容纳的数据，并更新记录 */
    pthread_mutex_lock(&jm->m_mutex);
    total_len = jm->m_head_len + jm->m_tail_len + 1; /* 保证结尾必须是 '\0' */
    jmb_first = jm->m_body.m_first;

    for (jmb = jm->m_body.m_first; jmb; jmb = jmb->m_next) {
        if ((total_len += jm->m_separator_len + jmb->m_len) > buf_size) {
            ret = true; break;
        }
        jmb_last = jmb;
    }

    if (ret) jm->m_body.m_first = jmb;
    else jm->m_body.m_first = jm->m_body.m_last = NULL;
    pthread_mutex_unlock(&jm->m_mutex);

    total_len = jm_tmp.m_head_len + jm_tmp.m_tail_len + 1; /* 保证结尾必须是 '\0' */
    if (total_len > buf_size) return false;
    if (jmb_last) jmb_last->m_next = NULL;

    /* 开始拼接数据 */
    total_len = 0;
    if (jm_tmp.m_head) {
        memcpy(buf, jm_tmp.m_head, jm_tmp.m_head_len);
        total_len += jm_tmp.m_head_len;
    }

    for (jmb = jmb_first; jmb; jmb = jmb_last) {
       if (total_len + jmb->m_len + jm_tmp.m_separator_len > buf_size  - jm_tmp.m_tail_len - 1) break;
       jmb_last = jmb->m_next;
       memcpy(buf + total_len, jmb->m_part, jmb->m_len);
       total_len += jmb->m_len;

       memcpy(buf + total_len, jm_tmp.m_separator, jm_tmp.m_separator_len);
       total_len += jm_tmp.m_separator_len;

       jm_tmp.m_part_free((void*)jmb->m_part);
       g_jm_free(jmb);
       is_join_body = true;
    }

    if (!is_join_body) return false; /* 缓冲区大小无法容纳body部分 */

    total_len -= jm_tmp.m_separator_len; /* 去除最后一个分隔符 */
    memcpy(buf + total_len, jm_tmp.m_tail, jm_tmp.m_tail_len);
    total_len += jm_tmp.m_tail_len;
    memset(buf + total_len, 0, 1); /* 保证结尾必须是'\0' */
    *read_len = total_len;
    return ret;
}

/* @func:
 *  释放管理器空间
 */
void join_mgr_free(join_mgr_t *jm)
{
    if (!jm) return ;
    join_mgr_body_t *jmb = NULL, *jmb_next = NULL;

    pthread_mutex_lock(&jm->m_mutex);
    for (jmb = jm->m_body.m_first; jmb; jmb = jmb_next) {
        jmb_next = jmb->m_next;
        jm->m_part_free((void*)jmb->m_part);
        g_jm_free(jmb);
    }
    g_jm_free(jm);
    pthread_mutex_destroy(&jm->m_mutex);
}

#include <assert.h>
int main()
{
    pthread_t pt[10];
    join_mgr_t *jm = NULL;
    const char *head = "header+";
    size_t head_len = strlen(head);
    const char *tail = "+tailer";
    size_t tail_len = strlen(tail);
    const char *separator = "-";
    size_t separator_len = strlen(separator);
    const char *part = "part";
    char buf[1024] = {0};
    size_t buf_size = sizeof(buf);
    size_t read_len = 0;
    size_t i = 0;
    bool ret = false;

    join_mgr_init(NULL, NULL);
    assert((jm = join_mgr_new(head, head_len, tail, tail_len, separator, separator_len, NULL)));

    join_mgr_dump(jm, true);
    for (i = 0; i < sizeof(pt)/ sizeof(pt[0]); i++) {
        pthread_create(&pt[i], NULL, ({
                    void* _(void *arg) {
                    char part_buf[64] = {0};
                    char *ptr = NULL;
                    int j = 0;
                    size_t rd_len = 0;
                    char buf_tmp[512] = {0};
                    size_t buf_tmp_len = sizeof(buf_tmp);

                    for (j = 0; j < 100; j++) {
                        snprintf(part_buf, sizeof(part_buf), "%s%d",part, j);
                        if (!(ptr = calloc(1, strlen(part_buf) + 1))) continue;
                        memcpy(ptr, part_buf, strlen(part_buf));
                        assert(join_mgr_add(jm, ptr, strlen(part_buf)));

                        if (j % 20 == 0) {
                            ret = join_mgr_join(jm, buf_tmp, buf_tmp_len, &rd_len);
                            if (rd_len > 0) {
                                assert(strlen(buf) == read_len);
                                MY_PRINTF("%s", buf_tmp);
                            }
                        }
                    }
                       return arg;
                    }; _;}), NULL);
    }

    for (i = 0; i < sizeof(pt)/ sizeof(pt[0]); i++) {
        pthread_join(pt[i], NULL);
    }

    do {
        ret = join_mgr_join(jm, buf, buf_size, &read_len);
        if (read_len > 0) {
            assert(strlen(buf) == read_len);
            MY_PRINTF("%s", buf);
        }
    } while (ret);

    join_mgr_free(jm);

    MY_PRINTF("ok");
    return 0;
}
