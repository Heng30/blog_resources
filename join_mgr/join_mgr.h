#ifndef _JOIN_MGR_H_
#define _JOIN_MGR_H_

#include <stdbool.h>

typedef void* (*join_mgr_alloc_t) (size_t size);
typedef void (*join_mgr_free_t) (void *ptr);
typedef void (*join_mgr_part_free_t) (void *ptr);

/* head+part1+separator+part2+separator+...+parnN+tail */
typedef struct _join_mgr_body {
    const void *m_part;
    size_t m_len;
    struct _join_mgr_body *m_next;
} join_mgr_body_t;

typedef struct _join_mgr {
    const void *m_head;
    const void *m_tail;
    const void *m_separator;
    size_t m_head_len;
    size_t m_tail_len;
    size_t m_separator_len; /* part之间的分割符 */
    pthread_mutex_t m_mutex;
    join_mgr_part_free_t m_part_free;
    struct {
        join_mgr_body_t *m_first;
        join_mgr_body_t *m_last;
    } m_body;
} join_mgr_t;

/* @func:
 *  打印信息
 */
void join_mgr_dump(join_mgr_t *jm, bool is_dump_body);

/* @func:
 *  初始化信息
 */
void join_mgr_init(join_mgr_alloc_t alloc, join_mgr_free_t dealloc);

/* @func:
 *  创建一个管理节点
 */
join_mgr_t* join_mgr_new(const void *head, size_t head_len, const void *tail, size_t tail_len,
                            const void *separator, size_t separator_len, join_mgr_part_free_t part_free);

/* @func:
 *  添加一个节点到管理器
 */
bool join_mgr_add(join_mgr_t *jm, const void *part, size_t part_len);

/* @func:
 *  拼接内存
 *  head+part1+separator+part2+separator+...+parnN+tail
 * @return:
 *  true: 还有内存没有拼接
 *  false: 内存都拼接完
 */
bool join_mgr_join(join_mgr_t *jm, void *buf, size_t buf_size, size_t *read_len);

/* @func:
 *  释放管理器空间
 */
void join_mgr_free(join_mgr_t *jm);

#endif

