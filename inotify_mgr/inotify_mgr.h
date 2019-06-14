/* @desc:
 *	1. 监听一个目录，对目录内的新增，删除，重命名，移入，移出文件操作进行监听；
 *	2. 保证经过上面的操作， 通过 inotify_mgr_path_get 函数获取到的目录都是正确的绝对路径
 *	3. sudo sysctl -w fs.inotify.max_user_watches=99999999
 *	4. sudo sysctl -w fs.inotify.max_queued_events=1638400
 *	事件丢失会造成错误，部分节点无法被删除，释放空间
 */

#ifndef _INOTIFY_MGR_H_
#define _INOTIFY_MGR_H_

#include <sys/inotify.h>
#include <stdbool.h>
#include "rb_tree_mgr.h"

typedef struct _inotify_mgr  inotify_mgr_t;

typedef void* (*inotify_mgr_alloc_t) (size_t size);
typedef void (*inotify_mgr_free_t) (void *ptr);
typedef void* (*inotify_mgr_func_t) (inotify_mgr_t *im, struct inotify_event *ev, void *arg);

typedef struct _wfd_node {
	int m_wfd;	/* 监听的文件或目录的描述符 */
    int m_parent_wfd; /* 所在目录的描述符 */
	const char *m_name;	/* 文件名或目录名 */
	rb_tree_mgr_t *m_child_wfd_tree; /* 子目录管理的描述符 */
} wfd_node_t;

struct _inotify_mgr {
	int m_ifd;
	int m_wfd; /* 根目录的描述符 */
	rb_tree_mgr_t *m_wfd_tree; /* 保存监听的套接字 */
	inotify_mgr_func_t m_func;
	void *m_arg;
};

/* @func:
 *	初始化管理器
 */
void inotify_mgr_init(inotify_mgr_alloc_t alloc, inotify_mgr_free_t dealloc);

/* @func:
 *	创建一个管理器
 */
inotify_mgr_t* inotify_mgr_new(const char *path, inotify_mgr_func_t func, void *arg);

/* @func:
 *	销毁一个管理器
 */
void inotify_mgr_free(inotify_mgr_t *im);

/* @func:
 *	处理inotify事件的回调函数
 */
void inotify_mgr_dispatch(inotify_mgr_t *im, char *buf, size_t buflen);

/* @func:
 *	监听的根目录是否删除了
 */
bool inotify_mgr_is_empty(inotify_mgr_t *im);

/* @func:
 *  获取一个绝对路径
 */
bool inotify_mgr_path_get(char *path, int path_len, inotify_mgr_t *im, struct inotify_event *ie);

/* @func:
 *	打印信息
 */
void inotify_mgr_dump(inotify_mgr_t *im, bool is_dump_wfd);

/* @func:
 *	打印mask值的含义
 */
void inotify_mgr_event_dump(struct inotify_event *ev);

#endif
