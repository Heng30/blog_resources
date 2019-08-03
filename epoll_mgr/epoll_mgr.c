#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/epoll.h>
#include <pthread.h>

#include "epoll_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define EPOLL_MGR_TRACE_LOG MY_PRINTF
#define EPOLL_MGR_DEBUG_LOG MY_PRINTF
#define EPOLL_MGR_INFO_LOG MY_PRINTF
#define EPOLL_MGR_WARN_LOG MY_PRINTF
#define EPOLL_MGR_ERROR_LOG MY_PRINTF

#define EPOLL_MGR_CREATE_SIZE 1024
#define EPOLL_MGR_SOCKET_STATUS_MASK 	(EPOLL_MGR_SOCKET_STATUS_RD | \
										EPOLL_MGR_SOCKET_STATUS_WR | \
										EPOLL_MGR_SOCKET_STATUS_PRI | \
										EPOLL_MGR_SOCKET_STATUS_ERR | \
										EPOLL_MGR_SOCKET_STATUS_HUP | \
										EPOLL_MGR_SOCKET_STATUS_ET | \
										EPOLL_MGR_SOCKET_STATUS_ONESHOT)
									

static void* _malloc2calloc(size_t size);
static epoll_mgr_alloc_t g_em_alloc = _malloc2calloc;
static epoll_mgr_free_t g_em_free = free;

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

/* @func:
 *	打印信息
 */
void epoll_mgr_dump(epoll_mgr_t *em, bool is_dump_member)
{
	if (!em) return ;
	int i = 0;

	EPOLL_MGR_TRACE_LOG("=============");
	EPOLL_MGR_TRACE_LOG("max_member: %d", em->m_max_member);
	EPOLL_MGR_TRACE_LOG("max_event: %d", em->m_max_event);
	EPOLL_MGR_TRACE_LOG("timeout: %d", em->m_timeout);
	EPOLL_MGR_TRACE_LOG("epollfd: %d", em->m_epollfd);
	EPOLL_MGR_TRACE_LOG("member: %p", em->m_member);
	if (!is_dump_member) goto out;
	for (i = 0; i < em->m_max_member; i++) {
		EPOLL_MGR_TRACE_LOG("++++++++++");
		EPOLL_MGR_TRACE_LOG("index: %d", i);
		EPOLL_MGR_TRACE_LOG("sock_status: %#x", em->m_member[i].m_sock_status);
		EPOLL_MGR_TRACE_LOG("cb: %p", em->m_member[i].m_cb);
		EPOLL_MGR_TRACE_LOG("arg: %p", em->m_member[i].m_arg);
		EPOLL_MGR_TRACE_LOG("++++++++++");
	}
	
out:
	EPOLL_MGR_TRACE_LOG("=============");
}

/* @func:
 *	初始化管理器
 */
void epoll_mgr_init(epoll_mgr_alloc_t alloc, epoll_mgr_free_t dealloc)
{
	if (!alloc || !dealloc) g_em_alloc = _malloc2calloc, g_em_free = free;
	else g_em_alloc = alloc, g_em_free = dealloc;
}

/* @func:
 *	创建管理节点
 */
epoll_mgr_t* epoll_mgr_new(int max_member, int max_event, int timeout)
{
	if (max_member <= 0 || max_event <= 0) return NULL;
	epoll_mgr_t *em = NULL;

	if (!(em = g_em_alloc(sizeof(epoll_mgr_t) + max_member * sizeof(epoll_mgr_member_t)))) {
		EPOLL_MGR_ERROR_LOG("g_em_alloc error, errno: %d - %s", errno, strerror(errno));
		return NULL;
	}
	
	if ((em->m_epollfd = epoll_create(EPOLL_MGR_CREATE_SIZE)) < 0) {
		EPOLL_MGR_ERROR_LOG("epoll_create error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

	em->m_max_member = max_member;
	em->m_max_event = max_event;
	em->m_timeout = timeout;
	em->m_member = (epoll_mgr_member_t*)(em + 1);
	
	return em;
	
err:
	if (em) g_em_free(em);
	return NULL;
}


/* @func:
 *	销毁管理器
 */
void epoll_mgr_free(epoll_mgr_t *em)
{
	if (!em) return ;
	int i = 0;

	for (i = 0; i < em->m_max_member; i++) {
		if (em->m_member[i].m_sock_status) {
			epoll_ctl(em->m_epollfd, EPOLL_CTL_DEL, i, NULL);
			close(i);
		}
	}
	close(em->m_epollfd);
	g_em_free(em);
}


/* @func:
 * 	关闭监听的套接字, 并初始化为空
 */
void epoll_mgr_clear(epoll_mgr_t *em, int sock)
{
	if (!em || sock < 0 || sock >= em->m_max_member) return ;

	if (em->m_member[sock].m_sock_status) {
		epoll_ctl(em->m_epollfd, EPOLL_CTL_DEL, sock, NULL);
		close(sock);
		memset(&em->m_member[sock], 0, sizeof(epoll_mgr_member_t));
	}
	
}

/* @func:
 *  添加套接字
 */
bool epoll_mgr_add(epoll_mgr_t *em, int sock, unsigned short status, epoll_mgr_callback_fn_t cb, void *arg)
{
	if (!em || sock < 0 || !cb) return false;
	if (sock >= em->m_max_member) return false;
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));

	em->m_member[sock].m_arg = arg;
	em->m_member[sock].m_cb = cb;
	em->m_member[sock].m_sock_status = EPOLL_MGR_SOCKET_STATUS_ACTIVE;

	if (status & EPOLL_MGR_SOCKET_STATUS_ET) ev.events |= EPOLLET;
	if (status & EPOLL_MGR_SOCKET_STATUS_RD) ev.events |= EPOLLIN;
	if (status & EPOLL_MGR_SOCKET_STATUS_WR) ev.events |= EPOLLOUT;
	if (status & EPOLL_MGR_SOCKET_STATUS_ONESHOT) ev.events |= EPOLLONESHOT;	
	
    ev.data.fd = sock;
	if (epoll_ctl(em->m_epollfd, EPOLL_CTL_ADD, sock, &ev) == -1) {
		EPOLL_MGR_WARN_LOG("epoll_ctl add error, epollfd: %d, sock: %d, errno: %d - %s", 
							em->m_epollfd, sock, errno, strerror(errno));
		memset(&em->m_member[sock], 0, sizeof(epoll_mgr_member_t));
		return false;
	}
	return true;
}

/* @func:
 *	修改节点的状态信息
 * @warn:
 *	cb不为NULL时才会修改原来节点的cb和arg信息
 */
bool epoll_mgr_mod(epoll_mgr_t *em, int sock, unsigned short status, epoll_mgr_callback_fn_t cb, void *arg)
{
	if (!em || sock < 0 || sock >= em->m_max_member) return false;
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));

	if (status & EPOLL_MGR_SOCKET_STATUS_CLOSE)
		__sync_fetch_and_or(&em->m_member[sock].m_sock_status, EPOLL_MGR_SOCKET_STATUS_CLOSE);

	if (cb) {
		em->m_member[sock].m_arg = arg;
		em->m_member[sock].m_cb = cb;
	}

	MY_PRINTF("mod: %#x", status);
	if (status & EPOLL_MGR_SOCKET_STATUS_ET) ev.events |= EPOLLET;
	if (status & EPOLL_MGR_SOCKET_STATUS_RD) ev.events |= EPOLLIN;
	if (status & EPOLL_MGR_SOCKET_STATUS_WR) ev.events |= EPOLLOUT;
	if (status & EPOLL_MGR_SOCKET_STATUS_ONESHOT) ev.events |= EPOLLONESHOT;
	MY_PRINTF("after mod: %#x", ev.events);
	
    ev.data.fd = sock;
	if (epoll_ctl(em->m_epollfd, EPOLL_CTL_MOD, sock, &ev) == -1) {
		EPOLL_MGR_WARN_LOG("epoll_ctl mod error, epollfd: %d, sock: %d, errno: %d - %s", 
							em->m_epollfd, sock, errno, strerror(errno));
		return false;
	}

	return true;
}

/* @func:
 *	从epoll中删除一个节点，并不会close套接字
 */
bool epoll_mgr_del(epoll_mgr_t *em, int sock)
{
	if (!em || sock < 0 || sock >= em->m_max_member) return false;
	
	if (epoll_ctl(em->m_epollfd, EPOLL_CTL_DEL, sock, NULL) == -1) {
		EPOLL_MGR_WARN_LOG("epoll_ctl add error, epollfd: %d, sock: %d, errno: %d - %s", 
							em->m_epollfd, sock, errno, strerror(errno));
		return false;
	}

	return true;
}

/* @func:
 *	获取套接字的状态
 */
unsigned short epoll_mgr_status(epoll_mgr_t *em, int sock)
{
	if (!em || sock < 0 || sock >= em->m_max_member) return false;
	return __sync_fetch_and_add(&em->m_member[sock].m_sock_status, 0);
}

/* @func:
 *	处理epoll事件
 */
void* epoll_mgr_dispatch(void *arg)
{
	if (!arg) return NULL;
	epoll_mgr_t *em = (epoll_mgr_t*)arg;
	int nfds = 0, i = 0, sock = 0;
	struct epoll_event *ev = NULL;
	unsigned short status = 0;
	
	if (!(ev = g_em_alloc(sizeof(struct epoll_event) * em->m_max_event))) {
		EPOLL_MGR_ERROR_LOG("g_em_alloc error, errno: %d - %s", errno, strerror(errno));
		return NULL;
	}

	while (true) {
		if (-1 == (nfds = epoll_wait(em->m_epollfd, ev, em->m_max_event, em->m_timeout))) {
			if (errno == EINTR) { errno = 0; continue; }
            EPOLL_MGR_ERROR_LOG("epoll_wait error: %d - %s", errno, strerror(errno));
			break;
		}
		
		for (i = 0; i < nfds; i++) {
			status = 0;
			sock = ev[i].data.fd;
		
			if (ev[i].events & EPOLLIN) status |= EPOLL_MGR_SOCKET_STATUS_RD;
			if (ev[i].events & EPOLLOUT) status |= EPOLL_MGR_SOCKET_STATUS_WR;
			if (ev[i].events & EPOLLPRI) status |= EPOLL_MGR_SOCKET_STATUS_PRI;
			if (ev[i].events & EPOLLERR) status |= EPOLL_MGR_SOCKET_STATUS_ERR;
			if (ev[i].events & EPOLLHUP) status |= EPOLL_MGR_SOCKET_STATUS_HUP;
			if (ev[i].events & EPOLLET) status |= EPOLL_MGR_SOCKET_STATUS_ET;
			if (ev[i].events & EPOLLONESHOT) status |= EPOLL_MGR_SOCKET_STATUS_ONESHOT;

			__sync_fetch_and_and(&em->m_member[sock].m_sock_status, ~EPOLL_MGR_SOCKET_STATUS_MASK);
			__sync_fetch_and_or(&em->m_member[sock].m_sock_status, status);
			
			if (em->m_member[sock].m_cb) 
				em->m_member[sock].m_cb(em, sock, em->m_member[sock].m_sock_status, em->m_member[sock].m_arg);
		}
	}

	if (ev) g_em_free(ev);
	return NULL;
}


/* ==========================test_start==========================*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <assert.h>
#include <fcntl.h>


#define LISTER_QUEUE 16
static int g_accept_client = 0;
static int g_close_client = 0;

bool _socket_set_nonblock(int fd) 
{
	int flags = fcntl(fd, F_GETFL, 0);
    if (-1 == flags) {
        MY_PRINTF("Crazy! fcntl error, errno: %d - %s", errno, strerror(errno));
        return false;
    }

	if (-1 == fcntl(fd, F_SETFL, flags | O_NONBLOCK)) {
		MY_PRINTF("Crazy! fcntl failed, errno: %d - %s", errno, strerror(errno));
        return false;
	}
	return true;
}

static int _tcp_server(unsigned short listen_port)
{
	int listen_fd = 0;
	const int on = 1;
	struct sockaddr_in server_addr;

	if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		MY_PRINTF("socket error, errno: %d - %s", errno, strerror(errno));
		return -1;
	}
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(listen_port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (-1 == setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on))) {
		MY_PRINTF("setsockopt error, errno: %d - %s", errno, strerror(errno));
	}
	
	if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		MY_PRINTF("socket error, errno: %d - %s", errno, strerror(errno));
		return -1;
	}
	
	if (listen(listen_fd, LISTER_QUEUE) < 0) {
		MY_PRINTF("socket error, errno: %d - %s", errno, strerror(errno));
		return -1;
	}
	MY_PRINTF("listen...\n");
	
	_socket_set_nonblock(listen_fd);
	return listen_fd;
}


static void* _cb_client(epoll_mgr_t *em, int sock, unsigned short status, void *arg)
{
	if (!em || sock < 0) return NULL;
	char buf[1024] = {0};
	const char *str = "i am server";
	int len = 0;
	MY_PRINTF("client status: %#x", status);

	if (status & EPOLL_MGR_SOCKET_STATUS_CLOSE) {
		MY_PRINTF("status close");
		epoll_mgr_del(em, sock);
		epoll_mgr_clear(em, sock);
		__sync_fetch_and_add(&g_close_client, 1);
		return NULL;
	}
	
	if (status & EPOLL_MGR_SOCKET_STATUS_RD) {
		MY_PRINTF("status rd");
		len = read(sock, buf, sizeof(buf) - 1);
		if (len > 0) MY_PRINTF("recv: %s", buf);
		else if (len <= 0) {
			MY_PRINTF("read error, errno: %d - %s", errno, strerror(errno));
			epoll_mgr_del(em, sock);
			epoll_mgr_clear(em, sock);
			__sync_fetch_and_add(&g_close_client, 1);
			return NULL;
		}
	}
	
	if (status & EPOLL_MGR_SOCKET_STATUS_WR) {
		MY_PRINTF("status wd");
		write(sock, str, strlen(str));
		epoll_mgr_mod(em, sock, EPOLL_MGR_SOCKET_STATUS_ET | EPOLL_MGR_SOCKET_STATUS_RD, NULL, NULL);
	}

	return arg;
}

struct  em_sock{
	epoll_mgr_t *m_em;
	int m_sock;
};

void *_rdwr_ctl(void *arg)
{
	if (!arg) return NULL;
	struct em_sock *em_s = (struct em_sock*)arg;
	time_t start = time((time_t*)NULL);
	time_t end = 0;
	int count = 0;

	while (true) {
		end = time((time_t*)NULL);
		if (end - start > 2) {
			count++;
			epoll_mgr_mod(em_s->m_em, em_s->m_sock, EPOLL_MGR_SOCKET_STATUS_ET| EPOLL_MGR_SOCKET_STATUS_RD | EPOLL_MGR_SOCKET_STATUS_WR, NULL, NULL);
			MY_PRINTF("active write and read");
			start = end;
		}		

		sleep(1);
		if (count == 2) {
			epoll_mgr_mod(em_s->m_em, em_s->m_sock, EPOLL_MGR_SOCKET_STATUS_WR | EPOLL_MGR_SOCKET_STATUS_CLOSE, NULL, NULL);
			MY_PRINTF("close sock: %d", em_s->m_sock);
			free(em_s);
			break;
		}
	}
	return arg;
}

static void* _cb(epoll_mgr_t *em, int sock, unsigned short status, void *arg)
{
	if (sock < 0 || !em) return NULL;
	
	int client_fd = 0;
	struct sockaddr_in client_addr;
	socklen_t len = sizeof(client_addr);
	char client_ip[40] = {0};
	unsigned short port = 0;
	pthread_t pt;
	struct em_sock *em_s;

	if (status & EPOLL_MGR_SOCKET_STATUS_RD) {
		while(true) {
			client_fd = accept(sock, (struct sockaddr*)&client_addr, &len);
			if (client_fd < 0) {
				if (errno == EINTR) { errno = 0; continue; }
				if (errno == EAGAIN || errno == EWOULDBLOCK) return NULL;
				MY_PRINTF("accept error, errno: %d - %s", errno, strerror(errno));
				return NULL;
			}
			
			if (!inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip))) {
				MY_PRINTF("inet_ntop error, errno: %d - %s", errno, strerror(errno));
				return NULL;
			}
			port = ntohs(client_addr.sin_port);
			MY_PRINTF("client ip: %s, port: %d, fd: %d", client_ip, port, client_fd);

			assert((epoll_mgr_add(em,  client_fd, status, _cb_client, NULL)));
			if ((em_s = calloc(1, sizeof(struct em_sock)))) {
				em_s->m_em = em,
				em_s->m_sock = client_fd,
				pthread_create(&pt, NULL, _rdwr_ctl, em_s);
			}
			
			MY_PRINTF("g_accept_client: %d", g_accept_client);
			MY_PRINTF("g_close_client: %d", g_close_client);
			__sync_fetch_and_add(&g_accept_client, 1);
		}

	}
	
	return arg;
}

static void* _tcp_client(void *arg)
{
	static const char *str = "hello, I am client";
	int client_fd = 0;
	struct sockaddr_in server_addr;
	int server_port = 0;
	char *server_ip = NULL;
	int count = 0;
	const int on = 1;
	
	server_ip = "0.0.0.0";
	server_port = 9999;

	while (true) {
		if (0 > (client_fd = socket(AF_INET, SOCK_STREAM, 0))) continue;
	
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(server_port);
		if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
			MY_PRINTF("inet_pton error, errno: %d - %s", errno, strerror(errno));
			continue;
		}

		if (-1 == setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on))) {
			MY_PRINTF("setsockopt error, errno: %d - %s", errno, strerror(errno));
		}
		
		if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
			MY_PRINTF("connect error, errno: %d - %s", errno, strerror(errno));
			continue;
		}
		MY_PRINTF("connect successfully...");
		
		if (write(client_fd, str, strlen(str)) != (ssize_t)strlen(str)) {
			MY_PRINTF("write error, errno: %d - %s", errno, strerror(errno));
		}
		close (client_fd);
		usleep(10);
		count++;
		if (count > 2) break;

	}
	return arg;

}

int main()
{
	int max_member = 2048, max_event = 256, timeout = 3;
	unsigned short status = EPOLL_MGR_SOCKET_STATUS_RD | EPOLL_MGR_SOCKET_STATUS_ET;
	epoll_mgr_t *em = NULL;
	int listen_fd = 0;
	pthread_t pt[20];
	size_t i = 0;
	
	assert((listen_fd = _tcp_server(9999)) >= 0);
	epoll_mgr_init(NULL, NULL);
	assert((em = epoll_mgr_new(max_member, max_event, timeout)));
	assert((epoll_mgr_add(em, listen_fd, status, _cb, NULL)));
//	epoll_mgr_dump(em, true);

	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
		pthread_create(&pt[i], NULL, _tcp_client, NULL);
	}

	epoll_mgr_dispatch(em);
	epoll_mgr_free(em);
	
	return 0;
}
