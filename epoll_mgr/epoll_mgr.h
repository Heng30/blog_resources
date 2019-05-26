#ifndef _EPOLL_MGR_H_
#define _EPOLL_MGR_H_

#include <stdbool.h>

#define EPOLL_MGR_SOCKET_STATUS_RD (1 << 0) /* 读操作 */
#define EPOLL_MGR_SOCKET_STATUS_WR (1 << 1) /* 写操作 */
#define EPOLL_MGR_SOCKET_STATUS_PRI (1 << 2) /* 有紧急数据可读*/
#define EPOLL_MGR_SOCKET_STATUS_ERR (1 << 3) /* 发生错误 */
#define EPOLL_MGR_SOCKET_STATUS_HUP (1 << 4) /* 文件描述符被挂断 */
#define EPOLL_MGR_SOCKET_STATUS_ET (1 << 5) /* 边缘触发 */
#define EPOLL_MGR_SOCKET_STATUS_ONESHOT (1 << 6) /* 只监听一次 */
#define EPOLL_MGR_SOCKET_STATUS_CLOSE (1 << 7) /* 删除套接字 */
#define EPOLL_MGR_SOCKET_STATUS_ACTIVE (1 << 8) /* 创建了套接字，套接字没有被释放 */

typedef struct _epoll_mgr epoll_mgr_t;
typedef void* (*epoll_mgr_alloc_t) (size_t size);
typedef void (*epoll_mgr_free_t) (void *ptr);
typedef void* (*epoll_mgr_callback_fn_t) (epoll_mgr_t *em, int sock, unsigned short status, void *arg);

typedef struct _epoll_mgr_member {
	unsigned short m_sock_status;
	epoll_mgr_callback_fn_t m_cb;
	void *m_arg;
} epoll_mgr_member_t;

struct _epoll_mgr {
	int m_max_member;
	int m_max_event; /* epoll_wait等待的最大事件数，不是epoll_create中的事件数 */
	int m_timeout;
	int m_epollfd;
	epoll_mgr_member_t *m_member;
};

/* @func:
 *	打印信息
 */
void epoll_mgr_dump(epoll_mgr_t *em, bool is_dump_member);

/* @func:
 *	初始化管理器
 */
void epoll_mgr_init(epoll_mgr_alloc_t alloc, epoll_mgr_free_t dealloc);

/* @func:
 *	创建管理节点
 */
epoll_mgr_t* epoll_mgr_new(int max_member, int max_event, int timeout);

/* @func:
 *	销毁管理器
 */
void epoll_mgr_free(epoll_mgr_t *em);

/* @func:
 * 	关闭监听的套接字, 并初始化为空
 */
void epoll_mgr_clear(epoll_mgr_t *em, int sock);

/* @func:
 *  添加套接字
 */
bool epoll_mgr_add(epoll_mgr_t *em, int sock, unsigned short status, epoll_mgr_callback_fn_t cb, void *arg);

/* @func:
 *	修改节点的状态信息
 * @warn:
 *	cb不为NULL时才会修改原来节点的cb和arg信息
 */
bool epoll_mgr_mod(epoll_mgr_t *em, int sock, unsigned short status, epoll_mgr_callback_fn_t cb, void *arg);

/* @func:
 *	从epoll中删除一个节点，并不会close套接字
 */
bool epoll_mgr_del(epoll_mgr_t *em, int sock);

/* @func:
 *	获取套接字的状态
 */
unsigned short epoll_mgr_status(epoll_mgr_t *em, int sock);

/* @func:
 *	处理epoll事件
 */
void* epoll_mgr_dispatch(void *arg);

#endif
