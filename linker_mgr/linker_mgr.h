#ifndef _LINKER_MGR_H_
#define _LINKER_MGR_H_

typedef void (*linker_free_cb) (void *item); /* 释放节点资源 */
typedef bool (*linker_equal_cb) (void *first, void *second); /* 判断两节点是否相等 */
typedef bool (*linker_walk_cb) (void *item); /* 遍历节点, 返回true时结束遍历;返回false时继续遍历 */
typedef bool (*linker_walk_set_cb) (void *item, void *arg); /* 遍历节点, 返回true时结束遍历;返回false时继续遍历 */

typedef struct _linker {
    void *m_item; /* 节点 */
    struct _linker *m_next;
} linker_t;

typedef struct _linker_mgr {
    bool m_is_mutex;
    pthread_rwlock_t m_rwlock;
    linker_t *m_linker; /* 链表头 */
    linker_free_cb m_free_cb; /* 释放资源的回调 */
} linker_mgr_t;

/* @func:
 *  获取一个管理器
 */
linker_mgr_t* linker_mgr_new(bool is_mutex, linker_free_cb free_cb);

/* @func:
 *  销毁一个管理器
 */
void linker_mgr_free(linker_mgr_t *lm);

/* @func:
 *  添加一个节点
 */
bool linker_mgr_add(linker_mgr_t *lm, void *item);

/* @func:
 *  删除一个元素
 */
bool linker_mgr_del(linker_mgr_t *lm, void *sample, linker_equal_cb equal_cb);

/* @func:
 *  判读一个节点是否存在
 */
bool linker_mgr_is_exsit(linker_mgr_t *lm, void *sample, linker_equal_cb equal_cb);

/* @func:
 *  遍历节点, 回调函数放回true,则不继续遍历
 */
bool linker_mgr_walk(linker_mgr_t *lm, linker_walk_cb walk_cb);

/* @func:
 *  遍历节点,并可以改变节点的信息, 回调放回true则不继续遍历
 */
bool linker_mgr_walk_set(linker_mgr_t *lm, linker_walk_set_cb walk_set_cb, void *arg);

#endif
