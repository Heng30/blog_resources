#ifndef _SYSLOG_MGR_H_
#define _SYSLOG_MAG_H_

#define SYSLOG_MGR_TYPE_LOCAL 0X1 /* 直接调用系统函数写入本地 */
#define SYSLOG_MGR_TYPE_NONLOCAL 0X2 /* 构造syslog报文，通过回调确定写入点 */
#define SYSLOG_MGR_TYPE_STDOUT 0X4 /* 输出到标准输出 */
#define SYSLOG_MGR_TYPE_DEBUG 0X8 /* 输出到程序的debug级别 */
#define SYSLOG_MGR_TYPE_PERROR 0X10 /* 输出到标准错误，同时需要设置SYSLOG_MGR_TYPE_LOCAL */

#define SYSLOG_TAG_SIZE 256

typedef void (*syslog_mgr_cb_t) (const char *msg, void *arg);

/* <90>Aug 27 11:29:39 root[30102]: content
 *	pri: <90>
 *	时间戳: Aug 27 11:29:39
 *	tag: root[30102]
 *	pid: [30102]
 *	msg: tag + ':' +  content
 */
typedef struct _syslog_mgr {
	int m_facility; /* 日志设备 */
	int m_level; /* 日志级别 */
	char m_tag[SYSLOG_TAG_SIZE]; 
	char m_hostname[SYSLOG_TAG_SIZE]; /* 主机名 */
	syslog_mgr_cb_t m_cb; /* 仅在SYSLOG_MGR_TYPE_NONLOCAL时回调 */
	void *m_arg; /* cb 的参数 */
	unsigned m_type; /* 日志写入类型，本地写入或构造syslog协议数据包 */
	struct {
		int m_domain; /* communication domain */
		int m_protocol; /* 协议族 */
		char m_localip[INET6_ADDRSTRLEN]; /* 本地绑定的ip */
		unsigned short m_localport; /* 本地绑定的端口 */
		char m_serverip[INET6_ADDRSTRLEN]; /* 服务器的ip */
		unsigned short m_serverport; /* 服务器的端口 */
	} m_sock;
} syslog_mgr_t;

/* @func:
 *	将类型字符串形式转换为整型
 */
int syslog_mgr_type_atoi(const char *name);

/* @func:
 *	将调试设备的字符串形式转换为整型
 */
int syslog_mgr_facility_atoi(const char *name);

/* @func:
 *	将调试级别的字符串形式转换为整型
 */
int syslog_mgr_level_atoi(const char *name);

/* @func:
 *	获取管理器
 */
syslog_mgr_t* syslog_mgr(void);

/* @func:
 *	设置管理器
 */
void syslog_mgr_init(void);

/* @func:
 *	销毁管理器
 */
void syslog_mgr_destroy(void);

/* @func:
 *	写入到本地或通过回调函数处理日志
 */
void syslog_mgr_do(const char *msg);

/* @func:
 *	打印信息
 */
void syslog_mgr_dump(void);

#endif
