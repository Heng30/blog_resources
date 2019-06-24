#ifndef _NET_UTIL_H_
#define _NET_UTIL_H_

#include <stdbool.h>
#include <net/if.h>

#define NET_UTIL_IPPROTO_UDP IPPROTO_UDP
#define NET_UTIL_IPPROTO_TCP IPPROTO_TCP

#define NET_UTIL_IPV4 0X4
#define NET_UTIL_IPV6 0X6


/* @func:
 *		设置套接字为阻塞
 */
bool net_util_set_block(int fd);

/* @func:
 *      设置套接字为非阻塞
 */
bool net_util_set_nonblock(int fd);

/* @func:
 *	打包一个地址
 */
bool net_util_addr_patch(const char *ip, unsigned short port, struct sockaddr_storage *storage);

/* @func:
 *	对应套接字进行ip和port绑定
 */
bool net_util_bind(int sock, const char *ip, unsigned short port);

/* @func:
 *	判断一个tcp连接是否成功
 */
bool net_util_tcp_is_connected(int fd);

/* @func:
 *	获取本地的ip和端口
 */
bool net_util_local_ipNport(int fd, char *ip, size_t ip_len, unsigned short *port, unsigned char ipv);

/* @func:
 *	获取ip和端口
 */
bool net_util_ipNport(struct sockaddr_storage *storage, char *ip, size_t ip_len, unsigned short *port, unsigned char ipv);

/* @func:
 *	创建一个服务器类型套接字
 */
int net_util_server(const char *ip, unsigned short port,  unsigned ip_proto, bool is_blocking);

/* @func:
 *	创建一个客户端类型套接字，cln_ip 或 cln_port 为空则由系统自动分配
 */
int net_util_client(const char *cln_ip, unsigned short cln_port, const char *svr_ip, unsigned short svr_port,  unsigned ip_proto, bool is_blocking);

#endif
