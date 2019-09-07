#ifndef _INTERFACE_INFO_H_
#define _INTERFACE_INFO_H_

#include <stdbool.h>
#include <linux/if_link.h>
#include <net/if.h>

/* 接口信息 */
typedef struct _interface_info {
	unsigned char m_family;
	char m_name[IFNAMSIZ]; /* 接口名字 */
	char m_ip[INET6_ADDRSTRLEN]; /* 接口ip地址 */
	char m_broadcast_ip[INET6_ADDRSTRLEN]; /* 接口广播地址 */
	char m_netmask[INET6_ADDRSTRLEN]; /* 接口子网掩码 */
	struct rtnl_link_stats m_packet_data; /* 接口数据信息 */
	struct _interface_info *m_next; 
} interface_info_t;

/* @func:
 *	获取同一个子网的ip地址
 */
bool interface_info_subnet_get(interface_info_t *head, const char *template_ip, const char *template_mask, char *ip, size_t ip_len);

/* @func:
 *	获取接口信息
 */
interface_info_t *interface_info_get(void);

/* @func:
 *	判断两个ip地址是否是同一子网
 */
bool interface_info_is_same_subnet(const char *ip_1, const char *netmask_1, const char *ip_2, const char *netmask_2);

/* @func:
 *	 打印接口信息
 */
void interface_info_dump(interface_info_t *head, bool is_dump_all);

/* @func:
 *	释放占用的空间
 */
void interface_info_free(interface_info_t *head);

#endif
