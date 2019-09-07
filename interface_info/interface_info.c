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

#include "interface_info.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define INTERFACE_INFO_WARN_LOG MY_PRINTF
#define INTERFACE_INFO_DEBUG_LOG MY_PRINTF
#define INTERFACE_INFO_ERROR_LOG MY_PRINTF
#define INTERFACE_INFO_INFO_LOG MY_PRINTF
#define INTERFACE_INFO_TRACE_LOG MY_PRINTF 

static void _interface_info_packet_data_dump(struct rtnl_link_stats *stats)
{
	if (!stats) return ;
	INTERFACE_INFO_TRACE_LOG("tx_packets: %u", stats->tx_packets);
	INTERFACE_INFO_TRACE_LOG("rx_packets: %u", stats->rx_packets);
	INTERFACE_INFO_TRACE_LOG("tx_bytes: %u", stats->tx_bytes);
	INTERFACE_INFO_TRACE_LOG("rx_bytes: %u", stats->rx_bytes);
}

static void _interface_info_dump(interface_info_t *info)
{
	if (!info) return ;
	INTERFACE_INFO_TRACE_LOG("================");
	INTERFACE_INFO_TRACE_LOG("family: %d", info->m_family);
	INTERFACE_INFO_TRACE_LOG("name: %s", info->m_name);
	INTERFACE_INFO_TRACE_LOG("ip: %s", info->m_ip);
	INTERFACE_INFO_TRACE_LOG("broadcast_ip: %s", info->m_broadcast_ip);
	INTERFACE_INFO_TRACE_LOG("netmask: %s", info->m_netmask);
	INTERFACE_INFO_TRACE_LOG("next: %p", info->m_next);
	_interface_info_packet_data_dump(&info->m_packet_data);
	INTERFACE_INFO_TRACE_LOG("================");
	INTERFACE_INFO_TRACE_LOG("\n");
}

/* @func:
 *	 打印接口信息
 */
void interface_info_dump(interface_info_t *head, bool is_dump_all)
{
	if (!head) return ;
	for ( ; head; head = head->m_next) {
		_interface_info_dump(head);
		if (!is_dump_all) break;
	}
}

/* @func:
 *	判断两个ipv4地址是否是同一子网
 */
static bool _is_same_subnet_v4(const char *ip_1, const char *netmask_1, const char *ip_2, const char *netmask_2)
{
	struct in_addr in, mask;
	unsigned int ret_1 = 0, ret_2 = 0;
	
	if (0 == inet_aton(ip_1, &in)) {
		INTERFACE_INFO_WARN_LOG("inet_aton error, errno: %d - %s", errno, strerror(errno));
		return false;
	}
	if (0 == inet_aton(netmask_1, &mask)) {
		INTERFACE_INFO_WARN_LOG("inet_aton error, errno: %d - %s", errno, strerror(errno));
		return false;
	}
	ret_1 = in.s_addr & mask.s_addr;
	
	if (0 == inet_aton(ip_2, &in)) {
		INTERFACE_INFO_WARN_LOG("inet_aton error, errno: %d - %s", errno, strerror(errno));
		return false;
	}
	if (0 == inet_aton(netmask_2, &mask)) {
		INTERFACE_INFO_WARN_LOG("inet_aton error, errno: %d - %s", errno, strerror(errno));
		return false;
	}
	ret_2 = in.s_addr & mask.s_addr;
	
	if (ret_1 == ret_2) return true;
	return false;
}

/* @func:
 *	判断两个ipv6地址是否是同一子网
 */
static bool _is_same_subnet_v6(const char *ip_1, const char *netmask_1, const char *ip_2, const char *netmask_2)
{
	struct in6_addr addr_1, addr_2, netmask_part_1, netmask_part_2;
	char ret_1 = 0, ret_2 = 0;
	size_t i = 0;

	if (inet_pton(AF_INET6, ip_1, (void*)&addr_1) != 1) {
		INTERFACE_INFO_WARN_LOG("inet_pton %s error, errno: %d - %s", ip_1, errno, strerror(errno));
		return false;
	}

	if (inet_pton(AF_INET6, ip_2, (void*)&addr_2) != 1) {
		INTERFACE_INFO_WARN_LOG("inet_pton %s error, errno: %d - %s", ip_2, errno, strerror(errno));
		return false;
	}

	if (inet_pton(AF_INET6, netmask_1, (void*)&netmask_part_1) != 1) {
		INTERFACE_INFO_WARN_LOG("inet_pton %s error, errno: %d - %s", netmask_1, errno, strerror(errno));
		return false;
	}

	if (inet_pton(AF_INET6, netmask_2, (void*)&netmask_part_2) != 1) {
		INTERFACE_INFO_WARN_LOG("inet_pton %s error, errno: %d - %s", netmask_2, errno, strerror(errno));
		return false;
	}

	for (i = 0; i < sizeof(struct in6_addr); i++) {
		ret_1 = netmask_part_1.s6_addr[i] & addr_1.s6_addr[i];
		ret_2 = netmask_part_2.s6_addr[i] & addr_2.s6_addr[i];
		if (ret_1 != ret_2) return false;
	}
	
	return true;	
}

/* @func:
 *	判断两个ip地址是否是同一子网
 */
bool interface_info_is_same_subnet(const char *ip_1, const char *netmask_1, const char *ip_2, const char *netmask_2)
{
	if (!ip_1 || !ip_2 || !netmask_1 || !netmask_2) return false;
	if (strcmp(netmask_1, netmask_2)) return false;
	unsigned ipv_1 = 0, ipv_2 = 0;
	
	ipv_1 = strstr(ip_1, ":") ? 6 : 4;
	ipv_2 = strstr(ip_2, ":") ? 6 : 4;
	if (ipv_1 != ipv_2) return false;
	
	if (4 == ipv_1) return _is_same_subnet_v4(ip_1, netmask_1, ip_2, netmask_2);
	
	return _is_same_subnet_v6(ip_1, netmask_1, ip_2, netmask_2);
}

/* @func:
 *	获取同一个子网的ip地址
 */
bool interface_info_subnet_get(interface_info_t *head, const char *template_ip, const char *template_mask, char *ip, size_t ip_len)
{
	if (!head || !template_ip || !template_mask || !ip) return NULL;
	interface_info_t *info = NULL;
	unsigned char ver = strstr(template_ip, ":") ? 6 : 4;
	unsigned char family = (ver == 4 ? AF_INET : AF_INET6);
	
	for (info = head; info; info = info->m_next) {
		if (info->m_family != family) continue;
		if (info->m_family == AF_INET) {
			if (!_is_same_subnet_v4(template_ip, template_mask, info->m_ip, info->m_netmask)) continue;
			strncpy(ip, info->m_ip, ip_len - 1);
			return true;
		} else if (info->m_family == AF_INET6) {
			if (!_is_same_subnet_v6(template_ip, template_mask, info->m_ip, info->m_netmask)) continue;
			strncpy(ip, info->m_ip, ip_len - 1);
			return true;
		}
	}

	return false;
}

/* @func:
 *	获取接口信息
 */
interface_info_t *interface_info_get(void)
{
	struct ifaddrs *ifaddr = NULL, *ifa = NULL;
	interface_info_t *head = NULL, *info = NULL;
	struct rtnl_link_stats *stats = NULL;
	int family = 0, ret = 0;
	char *ptr = NULL;
	char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
		INTERFACE_INFO_ERROR_LOG("getifaddrs error, errno: %d - %s", errno, strerror(errno));
		return NULL;
    }

    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
		
		if (!(info = (interface_info_t*)calloc(1, sizeof(interface_info_t)))) {
			INTERFACE_INFO_WARN_LOG("calloc error, errno: %d - %s", errno, strerror(errno));
			goto err;
		}
		
		if (head) info->m_next = head;
		head = info;
		
		family = head->m_family = ifa->ifa_addr->sa_family;
		strncpy(head->m_name, ifa->ifa_name, sizeof(head->m_name) - 1);
	
        if (family == AF_INET || family == AF_INET6) {
            ret = getnameinfo(ifa->ifa_addr, (family == AF_INET) ? sizeof(struct sockaddr_in) :
                    sizeof(struct sockaddr_in6), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (ret != 0) {
                INTERFACE_INFO_WARN_LOG("getnameinfo ip error, errno: %d - %s", ret , gai_strerror(ret));
            } else {
            	strncpy(head->m_ip, host, sizeof(head->m_ip) - 1);
				if (family == AF_INET6) {
					if ((ptr = strrchr(head->m_ip, '%'))) *ptr = '\0';
				}
            }
			
            ret = getnameinfo(ifa->ifa_netmask, (family == AF_INET) ? sizeof(struct sockaddr_in) :
                    sizeof(struct sockaddr_in6), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if (ret != 0) {
                INTERFACE_INFO_WARN_LOG("getnameinfo netmask error, errno: %d - %s", ret , gai_strerror(ret));
            } else strncpy(head->m_netmask, host, sizeof(head->m_netmask) - 1);
			
            ret = getnameinfo(ifa->ifa_broadaddr, (family == AF_INET) ? sizeof(struct sockaddr_in) :
                    sizeof(struct sockaddr_in6), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if (ret != 0) {
                INTERFACE_INFO_WARN_LOG("getnameinfo broadcast error, errno: %d - %s", ret , gai_strerror(ret));
            } else strncpy(head->m_broadcast_ip, host, sizeof(head->m_broadcast_ip) - 1);
        } else if (family == AF_PACKET && ifa->ifa_data != NULL) {
            stats = ifa->ifa_data;
			head->m_packet_data.tx_packets = stats->tx_packets;
			head->m_packet_data.rx_packets = stats->rx_packets;
			head->m_packet_data.tx_bytes = stats->tx_bytes;
			head->m_packet_data.rx_bytes = stats->rx_bytes;
        }
    }

err:
    freeifaddrs(ifaddr);
    return head;
}

/* @func:
 *	释放占用的空间
 */
void interface_info_free(interface_info_t *head)
{
	if (!head) return ;
	interface_info_t *ii = NULL;
	
	for ( ; head; head = ii) {
		ii =  head->m_next;
		free((void*)head);
	}
}


#if 1
#include <assert.h>

int main()
{
	interface_info_t *info = NULL;
	assert((info = interface_info_get()));
	interface_info_dump(info, true);

	assert(interface_info_is_same_subnet("3000:21::", "ffff:ffff::", "3000:21::", "ffff:ffff::"));
	assert(!interface_info_is_same_subnet("3000:1::", "ffff:ffff::", "3000:21::", "ffff:ffff::"));
	assert(interface_info_is_same_subnet("3000:21::ff", "ffff:ffff::", "3000:21:ef::", "ffff:ffff::"));
	assert(!interface_info_is_same_subnet("3000:21::", "ffff:fffa::", "3000:21::", "ffff:ffff::"));
	assert(interface_info_is_same_subnet("3000:21::", "ffff:fffa::", "3000:21::", "ffff:fffa::"));
	
	assert(interface_info_is_same_subnet("192.168.1.2", "255.255.255.0", "192.168.1.45", "255.255.255.0"));
	assert(!interface_info_is_same_subnet("192.168.1.2", "255.255.2.0", "192.168.1.45", "255.255.255.0"));
	assert(!interface_info_is_same_subnet("192.168.2.2", "255.255.255.0", "192.168.1.45", "255.255.255.0"));
	assert(interface_info_is_same_subnet("192.168.2.200", "255.255.8.0", "192.168.2.45", "255.255.8.0"));

	interface_info_free(info);
	MY_PRINTF("OK");
	return 0;
}

#endif