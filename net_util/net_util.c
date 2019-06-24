#define _GNU_SOURCE     /* To get defns of NI_MAXSERV and NI_MAXHOST */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>

#include "net_util.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define NET_UTIL_WARN_LOG MY_PRINTF
#define NET_UTIL_DEBUG_LOG MY_PRINTF
#define NET_UTIL_ERROR_LOG MY_PRINTF
#define NET_UTIL_INFO_LOG MY_PRINTF
#define NET_UTIL_TRACE_LOG MY_PRINTF 

#define LISTEN_MAX 128

/* @func:
 *		设置套接字为阻塞
 */
bool net_util_set_blocking(int fd)
{
	if (fd < 0) return false;
	int flags = 0;
	
	if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
        NET_UTIL_WARN_LOG("fcntl error, errno: %d - %s", errno, strerror(errno));
        return false;
    }

	if (-1 == fcntl(fd, F_SETFL, flags & ~O_NONBLOCK)) {
		NET_UTIL_WARN_LOG("fcntl failed,error: %d - %s", errno, strerror(errno));
		return false;
	}
	return true;
}

/* @func:
 *      设置套接字为非阻塞
 */
bool net_util_set_nonblocking(int fd) 
{
	if (fd < 0) return false;
	int flags = 0;
	
	if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
        NET_UTIL_WARN_LOG("fcntl error, errno: %d - %s", errno, strerror(errno));
        return false;
    }

	if (-1 == fcntl(fd, F_SETFL, flags | O_NONBLOCK)) {
		NET_UTIL_WARN_LOG("fcntl failed, errno: %d - %s", errno, strerror(errno));
        return false;
	}
	return true;
}

/* @func:
 *	打包一个地址
 */
bool net_util_addr_patch(const char *ip, unsigned short port, struct sockaddr_storage *storage)
{
	if (!ip || !port || !storage) return false;
	struct sockaddr_in *in = (struct sockaddr_in*)storage;
	struct sockaddr_in6 *in6 = (struct sockaddr_in6*)storage;
	int ver = strstr(ip, ":") ? 6 : 4;
	
	if (ver == 4) {
		in->sin_family = AF_INET;
		in->sin_port = htons(port);
		if (inet_pton(AF_INET, ip, &in->sin_addr) != 1) {
			NET_UTIL_WARN_LOG("inet_pton error, errno: %d - %s", errno, strerror(errno));
			return false;
		}
	} else {
		in6->sin6_family = AF_INET6;
		in6->sin6_port = htons(port);
		if (inet_pton(AF_INET6, ip, &in6->sin6_addr) != 1) {
			NET_UTIL_WARN_LOG("inet_pton error, errno: %d - %s", errno, strerror(errno));
			return false;
		}
	}

    return true;
}

/* @func:
 *	对应套接字进行ip和port绑定
 */
bool net_util_bind(int sock, const char *ip, unsigned short port)
{
	if (!ip || sock < 0 || !port) return false;	
	struct sockaddr_storage storage;
	int ver = strstr(ip, ":") ? 6 : 4;
	socklen_t len = 0;
	const int on = 0;

	if (ver == 4) len = sizeof(struct sockaddr_in);
	else len = sizeof(struct sockaddr_in6);
	
	if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on))) {
		NET_UTIL_WARN_LOG("setsockopt error, errno: %d - %s", errno, strerror(errno));
	}
	
	net_util_addr_patch(ip, port, &storage);
	if (bind(sock, (struct sockaddr*)&storage, len) != -1) return true;	
	
	NET_UTIL_WARN_LOG("bind %s:%d error, errno: %d - %s", ip, port, errno, strerror(errno));
	return false;
}

/* @func:
 *	判断一个tcp连接是否成功, 套接字要是非阻塞状态的
 */
bool net_util_tcp_is_connected(int fd)
{
	if (fd < 0) return false;
	char buf[1];

again:
	if (0 > write(fd, buf, 0)) {
		if (errno == EINTR) {
			errno = 0; goto again;
		}
		
		NET_UTIL_WARN_LOG("tcp fd:%d is not connected, errno: %d - %s", fd,  errno, strerror(errno));
		return false;
	} 
	return true;
}

/* @func:
 *	获取本地的ip和端口
 */
bool net_util_local_ipNport(int fd, char *ip, size_t ip_len, unsigned short *port, unsigned char ipv)
{
	if (!ip || fd < 0 || !ip_len || !port) return -1;
	struct sockaddr_in name;
	struct sockaddr_in6 name6;
	socklen_t namelen = sizeof(name);
	socklen_t namelen6 = sizeof(name6);
	memset(ip, 0, ip_len);

	if (NET_UTIL_IPV4 == ipv) {
		if (getsockname(fd, (struct sockaddr*)&name, &namelen) != 0) {
			NET_UTIL_WARN_LOG("getsockname error, errno: %d - %s", errno, strerror(errno));
			return false;
		}
		
		if (!inet_ntop(AF_INET, &name.sin_addr, ip, ip_len)) {
			NET_UTIL_WARN_LOG("inet_ntop error, errno: %d - %s", errno, strerror(errno));
			return false;
		}
		*port = ntohs(name.sin_port);
	} else {
		if (getsockname(fd, (struct sockaddr*)&name6, &namelen6) != 0) {
			NET_UTIL_WARN_LOG("getsockname error, errno: %d - %s", errno, strerror(errno));
			return false;
		}
		
		if (!inet_ntop(AF_INET6, &name6.sin6_addr, ip, ip_len)) {
			NET_UTIL_WARN_LOG("inet_ntop error, errno: %d - %s", errno, strerror(errno));
			return false;
		}
		*port =  ntohs(name6.sin6_port);
	}
	return true;
}

/* @func:
 *	获取ip和端口
 */
bool net_util_ipNport(struct sockaddr_storage *storage, char *ip, size_t ip_len, unsigned short *port, unsigned char ipv)
{
	if (!storage || !ip || !ip_len || !port) return -1;
	struct sockaddr_in *name = NULL;
	struct sockaddr_in6 *name6 = NULL;
	
	if (NET_UTIL_IPV4 == ipv) {
		name = (struct sockaddr_in*)storage;
		if (!inet_ntop(AF_INET, &name->sin_addr, ip, ip_len)) {
			NET_UTIL_WARN_LOG("inet_ntop error, errno: %d - %s", errno, strerror(errno));
			return false;
		}
		*port = ntohs(name->sin_port);
	} else {
		name6 = (struct sockaddr_in6*)storage;
		if (!inet_ntop(AF_INET6, &name6->sin6_addr, ip, ip_len)) {
			NET_UTIL_WARN_LOG("inet_ntop error, errno: %d - %s", errno, strerror(errno));
			return false;
		}
		*port =  ntohs(name6->sin6_port);
	}
	return true;
}

/* @func:
 *	初始化ipv4地址
 */
static int _net_util_init_tcp_ipv4(const char *ip, unsigned short port, struct sockaddr_in *addr_in)
{
	if (!ip || !addr_in || !port) return -1;
	int sock = 0;

	if (inet_pton(AF_INET, ip, &addr_in->sin_addr) != 1) {
		NET_UTIL_WARN_LOG("inet_pton %s:%d error, errno: %d - %s", ip, port, errno, strerror(errno));
		return -1;
	}
	addr_in->sin_family = AF_INET;
	addr_in->sin_port = htons(port);

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		NET_UTIL_WARN_LOG("socket error, errno: %d - %s", errno, strerror(errno));
		return -1;
	}

	return sock;
}

/* @func:
 * 	初始化ipv6地址 
 */
static int _net_util_init_tcp_ipv6(const char *ip, unsigned short port, struct sockaddr_in6 *addr_in6)
{
	if (!ip || !addr_in6 || !port) return -1;
	int sock = 0;
	
	if (inet_pton(AF_INET6, ip, &addr_in6->sin6_addr) != 1) {
		NET_UTIL_WARN_LOG("inet_pton %s:%d error, errno: %d - %s", ip, port, errno, strerror(errno));
		return -1;
	}
	addr_in6->sin6_family = AF_INET6;
	addr_in6->sin6_port = htons(port);

	if ((sock = socket(AF_INET6,SOCK_STREAM,IPPROTO_TCP)) < 0) {
		NET_UTIL_WARN_LOG("socket error, errno: %d - %s", errno, strerror(errno));
		return -1;
	}

	return sock;
}

/* @func:
 *	初始化ipv4的udp地址 */
static int _net_util_init_udp_ipv4(const char *ip, unsigned short port, struct sockaddr_in *addr_in)
{
	if (!ip || !addr_in || !port) return -1;
	int sock = 0;

	if (inet_pton(AF_INET, ip, &addr_in->sin_addr) != 1) {
		NET_UTIL_WARN_LOG("inet_pton %s:%d error, errno: %d -%s", ip, port, errno, strerror(errno));
		return -1;
	}
	addr_in->sin_family = AF_INET;
	addr_in->sin_port = htons(port);

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		NET_UTIL_WARN_LOG("socket error, errno: %d - %s", errno, strerror(errno));
		return -1;
	}	

	return sock;
}

/* @func:
 *	初始化一个ipv6地址
 */
static int _net_util_init_udp_ipv6(const char *ip, unsigned short port, struct sockaddr_in6 *addr_in6)
{
	if (!ip || !addr_in6 || !port) return -1;
	int sock = 0;

	if (inet_pton(AF_INET6, ip, &addr_in6->sin6_addr) != 1) {
		NET_UTIL_WARN_LOG("inet_pton %s:%d error, errno: %d - %s", ip, port, errno, strerror(errno));
		return -1;
	}

	addr_in6->sin6_family = AF_INET6;
	addr_in6->sin6_port = htons(port);

	if ((sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		NET_UTIL_WARN_LOG("socket error, errno: %d - %s", errno, strerror(errno));
		return -1;
	}

	return sock;
}

/* @func:
 *	创建一个tcp服务端类型的监听套接字
 */
static int _net_util_tcp_server(const char *ip, unsigned short port, bool is_blocking)
{
	if (!ip || !port) return -1;
	int sock = 0;
	socklen_t len = 0;
	struct sockaddr_storage storage;
	struct sockaddr_in *addr_in = NULL;
	struct sockaddr_in6 *addr_in6 = NULL;
	int ver = strstr(ip, ":") ? 6 : 4;
	const int on = 1;
	
	if (ver == 4)  {
		addr_in = (struct sockaddr_in *)&storage;
		sock = _net_util_init_tcp_ipv4(ip, port, addr_in);
		len = sizeof(struct sockaddr_in);
	} else {
		addr_in6 = (struct sockaddr_in6 *)&storage;
		sock = _net_util_init_tcp_ipv6(ip, port, addr_in6);
		len = sizeof(struct sockaddr_in6);
	}
	
	if (sock < 0) return -1;
	
	if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on))) {
		NET_UTIL_WARN_LOG("setsockopt error, errno: %d - %s", errno, strerror(errno));
	}

	if (!is_blocking) net_util_set_nonblocking(sock);
	
	if (bind(sock, (struct sockaddr*)&storage, len) == -1) {
		NET_UTIL_WARN_LOG("bind %s:%d error, errno: %d - %s", ip, port, errno, strerror(errno)); 
		close(sock);
		return -1;
	}
	
	listen(sock, LISTEN_MAX); 
	return sock;
}

/* @func:
 *	创建一个服务端类型的udp监听套接字
 */
static int _net_util_udp_server(const char *ip, unsigned short port, bool is_blocking)
{
	if (!ip || !port) return -1;
	int sock = 0;
	socklen_t len = 0;
	struct sockaddr_storage storage;
	struct sockaddr_in *addr_in = NULL;
	struct sockaddr_in6 *addr_in6 = NULL;
	int ver = strstr(ip, ":") ? 6 : 4;
	const int on = 1;

	if (ver == 4) {
		addr_in = (struct sockaddr_in *)&storage;
		sock = _net_util_init_udp_ipv4(ip, port, addr_in);
		len = sizeof(struct sockaddr_in);
	} else {
		addr_in6 = (struct sockaddr_in6 *)&storage;
		sock = _net_util_init_udp_ipv6(ip ,port, addr_in6);
		len = sizeof(struct sockaddr_in6);
	}
	
	if (sock < 0) return -1;

	if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on))) {
		NET_UTIL_WARN_LOG("setsockopt error, errno: %d - %s", errno, strerror(errno));
	}

	if (!is_blocking) net_util_set_nonblocking(sock);

	if (bind(sock, (struct sockaddr *)&storage, len) == -1) {		 
		NET_UTIL_WARN_LOG("bind %s:%d error, errno: %d - %s", ip, port, errno, strerror(errno)); 
		close(sock);
		return -1;
	}
		
	return sock;
}


/* @func: 
 *	创建一个tcp客户端类型的套接字
 */
static int _net_util_tcp_client(const char *cln_ip, unsigned short cln_port, const char *svr_ip, unsigned short svr_port, bool is_blocking)
{
	if (!svr_ip || !svr_port) return -1;
	int sock = 0;
	socklen_t svr_len = 0, cln_len = 0;	
	struct sockaddr_storage svr_storage, cln_storage;
	struct sockaddr_in *addr_in = NULL;
	struct sockaddr_in6 *addr_in6 = NULL;
	int svr_ver = strstr(svr_ip, ":") ? 6 : 4;
	const int on = 1;

again:	
	if (svr_ver == 4) {
		addr_in = (struct sockaddr_in*)&svr_storage;
		sock = _net_util_init_tcp_ipv4(svr_ip, svr_port, addr_in);
		svr_len = sizeof(struct sockaddr_in);
	} else {
		addr_in6 = (struct sockaddr_in6*)&svr_storage;
		sock = _net_util_init_tcp_ipv6(svr_ip, svr_port, addr_in6);
		svr_len = sizeof(struct sockaddr_in6);
	}
	
	if (sock < 0) return -1;

	if (cln_ip && cln_port > 0) {
		cln_len = strstr(cln_ip, ":") ?  sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
		if (net_util_addr_patch(cln_ip, cln_port, &cln_storage)) {
			if (bind(sock, (struct sockaddr*)&cln_storage, cln_len) == -1) {		 
				NET_UTIL_WARN_LOG("bind %s:%d error, errno: %d - %s", cln_ip, cln_port, errno, strerror(errno)); 
				close(sock);
				return -1;
			}
		}
	}
	
	if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on))) {
		NET_UTIL_WARN_LOG("setsockopt error, errno: %d - %s", errno, strerror(errno));
	}
	
#if 0
	if (-1 == (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,(const char *)&on, sizeof(on)))) {
		NET_UTIL_WARN_LOG("setsockopt error, errno: %d - %s", errno, strerror(errno));
	}
#endif

	if (!is_blocking) net_util_set_nonblocking(sock);
	
	if (connect(sock, (struct sockaddr *)&svr_storage, svr_len) == -1) {
		if (errno == EINTR) {
			errno = 0; goto again;
		} else if (errno == EINPROGRESS) return sock;

		NET_UTIL_WARN_LOG("connect %s:%d error, errno: %d - %s", svr_ip, svr_port, errno, strerror(errno));
		close(sock);
		return -1;
	}
	return sock;
}

/* @func:
 *	创建一个udp套接口
 */
static int _net_util_udp_client(const char *cln_ip, unsigned short cln_port, const char *svr_ip, unsigned short svr_port, bool is_blocking)
{
	if (!svr_ip || !svr_port) return -1;
	int sock = 0;
	socklen_t svr_len = 0, cln_len = 0;	
	struct sockaddr_storage svr_storage, cln_storage;
	struct sockaddr_in *addr_in = NULL;
	struct sockaddr_in6 *addr_in6 = NULL;
	int svr_ver = strstr(svr_ip, ":") ? 6 : 4;
	const int on = 1;

again:	
	if (svr_ver == 4) {
		addr_in = (struct sockaddr_in*)&svr_storage;
		sock = _net_util_init_udp_ipv4(svr_ip, svr_port, addr_in);
		svr_len = sizeof(struct sockaddr_in);
	} else {
		addr_in6 = (struct sockaddr_in6*)&svr_storage;
		sock = _net_util_init_udp_ipv6(svr_ip, svr_port, addr_in6);
		svr_len = sizeof(struct sockaddr_in6);
	}
	
	if (sock < 0) return -1;

	if (cln_ip && cln_port > 0) {
		cln_len = strstr(cln_ip, ":") ?  sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
		if (net_util_addr_patch(cln_ip, cln_port, &cln_storage)) {
			if (bind(sock, (struct sockaddr*)&cln_storage, cln_len) == -1) {		 
				NET_UTIL_WARN_LOG("bind %s:%d error, errno: %d - %s", cln_ip, cln_port, errno, strerror(errno)); 
				close(sock);
				return -1;
			}
		}
	}

	if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on))) {
		NET_UTIL_WARN_LOG("setsockopt error, errno: %d - %s", errno, strerror(errno));
	}

	if (!is_blocking) net_util_set_nonblocking(sock);

	if (connect(sock, (struct sockaddr *)&svr_storage, svr_len) == -1) {
		if (errno == EINTR) {
			errno = 0; goto again;
		} else if (errno == EINPROGRESS) return sock;

		NET_UTIL_WARN_LOG("connect %s:%d error, errno: %d - %s", svr_ip, svr_port, errno, strerror(errno));
		close(sock);
		return -1;
	}

	return sock;
}

/* @func:
 *	创建一个服务器类型套接字
 */
int net_util_server(const char *ip, unsigned short port,  unsigned ip_proto, bool is_blocking)
{
	if (!ip || !port) return -1;
	
	switch (ip_proto) {
		case NET_UTIL_IPPROTO_UDP: return _net_util_udp_server(ip, port, is_blocking); 
		case NET_UTIL_IPPROTO_TCP: return _net_util_tcp_server(ip, port, is_blocking);
		default: NET_UTIL_WARN_LOG("ip_proto invalid: %d", ip_proto);
	}
	return -1;
}

/* @func:
 *	创建一个客户端类型套接字，cln_ip 或 cln_port 为空则由系统自动分配
 */
int net_util_client(const char *cln_ip, unsigned short cln_port, const char *svr_ip, unsigned short svr_port,  unsigned ip_proto, bool is_blocking)
{
	if (!svr_ip || !svr_port) return -1;
	
	switch (ip_proto) {
		case NET_UTIL_IPPROTO_UDP: return _net_util_udp_client(cln_ip, cln_port, svr_ip, svr_port, is_blocking); 
		case NET_UTIL_IPPROTO_TCP: return _net_util_tcp_client(cln_ip, cln_port, svr_ip, svr_port, is_blocking);
		default: NET_UTIL_WARN_LOG("ip_proto invalid: %d", ip_proto);
	}
	return -1;
}


#if 1

#include <assert.h>
#include <pthread.h>

int main()
{
	pthread_t pt[8];
	size_t i = 0;
	
	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
		pthread_create(&pt[i], NULL, ({
			void* _(void *arg) {	
				const char *svr_ipv4 = "0.0.0.0";
				const char *svr_ipv6 = "::";
				const char *cln_ipv4 = "127.0.0.1";
				const char *cln_ipv6 = "::1";
				unsigned short svr4_port = 60000;
				unsigned short svr6_port = 60001;
				unsigned short cln4_port = 60002;
				unsigned short cln6_port = 60003;
				int sock, cln_sock = 0;
				struct sockaddr_in addr4;
				struct sockaddr_in6 addr6;
				socklen_t len4 = sizeof(addr4), len6 = sizeof(addr6);
				char ip[40] = {0};
				unsigned short port = 0;
				int pt_index = (int)arg;
				const char *svr_str = "Hi, I am server";
				const char *cln_str = "Hi, I am client";
				char buf[1024] = {0};
				

				if (pt_index == 0) {
					assert((sock = net_util_server(svr_ipv4, svr4_port, NET_UTIL_IPPROTO_TCP, true)) > 0);
					while ((cln_sock = accept(sock, (struct sockaddr*)&addr4, &len4)) >= 0) {
						net_util_ipNport((struct sockaddr_storage*)&addr4, ip, sizeof(ip), &port, NET_UTIL_IPV4); 
						MY_PRINTF("pt_index: %d, cln_fd: %d, cln_ip: %s, cln_port: %d, svr_ip: %s, svr_port: %d", pt_index, cln_sock, ip, port, svr_ipv4, svr4_port);
						read(cln_sock, buf, sizeof(buf));
						MY_PRINTF("pt_index: %d, recv msg: %s", pt_index, buf);
						write(cln_sock, svr_str, strlen(svr_str));
						close(cln_sock);
					}
				} else if (pt_index == 1) {
					assert((sock = net_util_server(svr_ipv4, svr4_port, NET_UTIL_IPPROTO_UDP, true)) > 0);
					while (recvfrom(sock, buf, sizeof(buf), 0, &addr4, &len4) > 0) {
						net_util_ipNport((struct sockaddr_storage*)&addr4, ip, sizeof(ip), &port, NET_UTIL_IPV4); 
						MY_PRINTF("pt_index: %d, cln_fd: %d, cln_ip: %s, cln_port: %d, svr_ip: %s, svr_port: %d", pt_index, sock, ip, port, svr_ipv4, svr4_port);
						MY_PRINTF("pt_index: %d, recv msg: %s", pt_index, buf);
						sendto(sock, svr_str, strlen(svr_str), 0, &addr4, len4);
					}
					close(sock);
				} else if (pt_index == 2) {
					sleep(1);
					assert((sock = net_util_client(cln_ipv4, cln4_port, svr_ipv4, svr4_port, NET_UTIL_IPPROTO_TCP, false)) > 0);
					net_util_local_ipNport(sock, ip, sizeof(ip), &port, NET_UTIL_IPV4); 
					MY_PRINTF("pt_index: %d, cln_fd: %d, cln_ip: %s, cln_port: %d, svr_ip: %s, svr_port: %d", pt_index, sock, ip, port, svr_ipv4, svr4_port);

					while (!net_util_tcp_is_connected(sock)) 
						;
					write(sock, cln_str, strlen(cln_str));				
					while (recvfrom(sock, buf, sizeof(buf), 0, &addr4, &len4) < 0) sleep(1);
					MY_PRINTF("pt_index: %d, recv msg: %s", pt_index, buf);
					close(sock);
				} else if (pt_index == 3) {
					sleep(1);
					assert((sock = net_util_client(cln_ipv4, cln4_port, svr_ipv4, svr4_port, NET_UTIL_IPPROTO_UDP, false)) > 0);
					net_util_local_ipNport(sock, ip, sizeof(ip), &port, NET_UTIL_IPV4); 
					MY_PRINTF("pt_index: %d, cln_fd: %d, cln_ip: %s, cln_port: %d, svr_ip: %s, svr_port: %d", pt_index, sock, ip, port, svr_ipv4, svr4_port);

					sleep(3);
					write(sock, cln_str, strlen(cln_str));
					while (recvfrom(sock, buf, sizeof(buf), 0, &addr4, &len4) < 0) sleep(1);
					MY_PRINTF("pt_index: %d recv msg: %s", pt_index, buf);
					close(sock);
				} else if (pt_index == 4) {
					assert((sock = net_util_server(svr_ipv6, svr6_port, NET_UTIL_IPPROTO_TCP, true)) > 0);
					while ((cln_sock = accept(sock, (struct sockaddr*)&addr6, &len6)) >= 0) {
						net_util_ipNport((struct sockaddr_storage*)&addr6, ip, sizeof(ip), &port, NET_UTIL_IPV6); 
						MY_PRINTF("pt_index: %d, cln_fd: %d, cln_ip: %s, cln_port: %d, svr_ip: %s, svr_port: %d", pt_index, cln_sock, ip, port, svr_ipv6, svr6_port);
						read(cln_sock, buf, sizeof(buf));
						MY_PRINTF("pt_index: %d, recv msg: %s", pt_index, buf);
						write(cln_sock, svr_str, strlen(svr_str));
						close(cln_sock);
					}
				} else if (pt_index == 5) {
					assert((sock = net_util_server(svr_ipv6, svr6_port, NET_UTIL_IPPROTO_UDP, true)) > 0);
					while (recvfrom(sock, buf, sizeof(buf), 0, &addr6, &len6) > 0) {
						net_util_ipNport((struct sockaddr_storage*)&addr6, ip, sizeof(ip), &port, NET_UTIL_IPV6); 
						MY_PRINTF("pt_index: %d, cln_fd: %d, cln_ip: %s, cln_port: %d, svr_ip: %s, svr_port: %d", pt_index, sock, ip, port, svr_ipv6, svr6_port);
						MY_PRINTF("pt_index: %d, recv msg: %s", pt_index, buf);
						sendto(sock, svr_str, strlen(svr_str), 0, &addr6, len6);
					}
					close(sock);
				} else if (pt_index == 6) {
					sleep(1);
					assert((sock = net_util_client(NULL, 0, svr_ipv6, svr6_port, NET_UTIL_IPPROTO_TCP, true)) > 0);
					net_util_local_ipNport(sock, ip, sizeof(ip), &port, NET_UTIL_IPV6); 
					MY_PRINTF("pt_index: %d, cln_fd: %d, cln_ip: %s, cln_port: %d, svr_ip: %s, svr_port: %d", pt_index, sock, ip, port, svr_ipv6, svr6_port);

					write(sock, cln_str, strlen(cln_str));
					while (recvfrom(sock, buf, sizeof(buf), 0, &addr6, &len6) < 0) sleep(1);
					MY_PRINTF("pt_index: %d, recv msg: %s", pt_index, buf);
					close(sock);
				} else if (pt_index == 7) {
					sleep(1);
					assert((sock = net_util_client(NULL, 0, svr_ipv6, svr6_port, NET_UTIL_IPPROTO_UDP, true)) > 0);
					net_util_local_ipNport(sock, ip, sizeof(ip), &port, NET_UTIL_IPV6); 
					MY_PRINTF("pt_index: %d, cln_fd: %d, cln_ip: %s, cln_port: %d, svr_ip: %s, svr_port: %d", pt_index, sock, ip, port, svr_ipv6, svr6_port);

					write(sock, cln_str, strlen(cln_str));
					while (recvfrom(sock, buf, sizeof(buf), 0, &addr6, &len6) < 0) sleep(1);
					MY_PRINTF("pt_index: %d, recv msg: %s", pt_index, buf);
					close(sock);
				} 
				return arg;
		}; _;}), (void*)i);
	}

	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) 
		pthread_join(pt[i], NULL);

	return 0;
}
#endif
