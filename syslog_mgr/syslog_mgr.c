#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>
#include <err.h>

#define SYSLOG_NAMES
#include <syslog.h>

#include "syslog_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define _TRACE_LOG MY_PRINTF
#define _DEBUG_LOG MY_PRINTF
#define _INFO_LOG MY_PRINTF
#define _WARN_LOG MY_PRINTF
#define _ERROR_LOG MY_PRINTF

#undef MAX_LINE
#define MAX_LINE 65536

typedef void* (*g_alloc_t) (size_t size);
typedef void (*g_free_t) (void *ptr);

static void* _malloc2calloc(size_t size);
static void _free(void *ptr);
static g_alloc_t g_alloc = _malloc2calloc;
static g_free_t g_free = _free;
static syslog_mgr_t g_syslog_mgr;

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

static void _free(void *ptr)
{
	if (!ptr) return;
	free(ptr);
}

static void _set_default(void)
{
	g_alloc = _malloc2calloc;
	g_free = _free;

	memset(&g_syslog_mgr, 0, sizeof(g_syslog_mgr));
	g_syslog_mgr.m_facility = LOG_USER;
	g_syslog_mgr.m_level = LOG_INFO;
	g_syslog_mgr.m_sock.m_domain = AF_INET;
	g_syslog_mgr.m_sock.m_protocol = IPPROTO_UDP;
	g_syslog_mgr.m_sock.m_localport = 0;
	g_syslog_mgr.m_sock.m_serverport = 514;
	g_syslog_mgr.m_cb = NULL;
	g_syslog_mgr.m_arg = NULL;
	g_syslog_mgr.m_type = SYSLOG_MGR_TYPE_LOCAL | SYSLOG_MGR_TYPE_STDOUT;
	gethostname(g_syslog_mgr.m_hostname, sizeof(g_syslog_mgr.m_hostname) - 1);
	snprintf(g_syslog_mgr.m_tag, sizeof(g_syslog_mgr.m_tag), "%s", getlogin());
	snprintf(g_syslog_mgr.m_sock.m_localip, sizeof(g_syslog_mgr.m_sock.m_localip), "%s", "0.0.0.0");
	snprintf(g_syslog_mgr.m_sock.m_serverip, sizeof(g_syslog_mgr.m_sock.m_serverip), "%s", "0.0.0.0");
}

/* @func:
 *	获取管理器
 */
syslog_mgr_t* syslog_mgr(void)
{
	return &g_syslog_mgr;
}

/* @func:
 *	设置管理器
 */
void syslog_mgr_init(void)
{
	_set_default();
}

/* @func:
 *	销毁管理器
 */
void syslog_mgr_destroy(void)
{
	_set_default();
}

/* @func:
 *	写入到本地或通过回调函数处理日志
 */
void syslog_mgr_do(const char *msg) 
{
	if (!msg) return ;
	time_t now;
	int option = 0;
    char buf[MAX_LINE], *tp = NULL;
	int pri = g_syslog_mgr.m_facility | g_syslog_mgr.m_level;
	
	if (g_syslog_mgr.m_type & SYSLOG_MGR_TYPE_STDOUT) {
		MY_PRINTF("%s", msg);
	}

	if (g_syslog_mgr.m_type & SYSLOG_MGR_TYPE_DEBUG) {
		_DEBUG_LOG("%s", msg);
	}

	if (g_syslog_mgr.m_type & SYSLOG_MGR_TYPE_LOCAL) {	
		option = LOG_PID;
		if (g_syslog_mgr.m_type & SYSLOG_MGR_TYPE_PERROR) option |= LOG_PERROR;
		openlog(g_syslog_mgr.m_tag, option, g_syslog_mgr.m_facility);
		syslog(pri, "%s", msg);
		closelog();
	}
	
    if ((g_syslog_mgr.m_type & SYSLOG_MGR_TYPE_NONLOCAL) && g_syslog_mgr.m_cb) {
		time((time_t*)&now);
        tp = ctime(&now) + 4;

        snprintf(buf, sizeof(buf), "<%d>%.15s %.256s %.256s[%d]: %s", 
        			pri, tp, g_syslog_mgr.m_hostname, g_syslog_mgr.m_tag,
        			getpid(), msg);
		
        g_syslog_mgr.m_cb(buf, g_syslog_mgr.m_arg);
    }

	return;
}

/* @func:
 *	将类型字符串形式转换为整型
 */
int syslog_mgr_type_atoi(const char *name)
{
	if (!name) return -1;
	if (isdigit(*name)) return (atoi(name));

	if (!strcasecmp(name, "LOCAL")) return SYSLOG_MGR_TYPE_LOCAL;
	else if (!strcasecmp(name, "NONLOCAL")) return SYSLOG_MGR_TYPE_NONLOCAL;
	else if (!strcasecmp(name, "STDOUT")) return SYSLOG_MGR_TYPE_STDOUT;
	else if (!strcasecmp(name, "DEBUG")) return SYSLOG_MGR_TYPE_DEBUG;
	else if (!strcasecmp(name, "PERROR")) return SYSLOG_MGR_TYPE_PERROR;
	else return -1;
}

/* @func:
 *	将调试级别的字符串形式转换为整型
 * @mapping
 * 	val: 0, name: emerg
 	val: 0, name: panic
	val: 1, name: alert
	val: 2, name: crit
	val: 3, name: err
	val: 3, name: error
	val: 4, name: warn
	val: 4, name: warnin
	val: 5, name: notice
	val: 6, name: info
	val: 7, name: debug
	val: 16, name: none
 */
int syslog_mgr_level_atoi(const char *name) 
{
	if (!name) return -1;
    CODE *c = NULL;

    if (isdigit(*name)) return (atoi(name));

    for (c = prioritynames; c->c_name; c++) {
#ifdef _DEBUG
		MY_PRINTF("val: %d, name: %s", c->c_val, c->c_name);
#endif
        if (!strcasecmp(name, c->c_name)) return (c->c_val | LOG_PRIMASK);
    }
    return -1;
}

/* @func:
 *	将调试设备的字符串形式转换为整型
 * @mapping
 *		
	val: 0, name: kern
	val: 8, name: user
	val: 16, name: mail
	val: 24, name: daemon
	val: 32, name: auth
	val: 32, name: security
	val: 40, name: syslog
	val: 48, name: lpr
	val: 56, name: news
	val: 64, name: uucp
	val: 72, name: cron
	val: 80, name: authpriv
	val: 88, name: ftp
	val: 128, name: local0
	val: 136, name: local1
	val: 144, name: local2
	val: 152, name: local3
	val: 160, name: local4
	val: 168, name: local5
	val: 176, name: local6
	val: 184, name: local7
	val: 192, name: mark
 */
int syslog_mgr_facility_atoi(const char *name) 
{
	if (!name) return -1;
    CODE *c = NULL;

    if (isdigit(*name)) return (atoi(name));

    for (c = facilitynames; c->c_name; c++) {
#ifdef _DEBUG
		MY_PRINTF("val: %d, name: %s", c->c_val, c->c_name);
#endif
        if (!strcasecmp(name, c->c_name)) return (c->c_val | LOG_FACMASK);
    }
    return -1;
}

/* @func:
 *	打印信息
 */
void syslog_mgr_dump(void)
{
	_TRACE_LOG("=================");	
	_TRACE_LOG("facility: %d", g_syslog_mgr.m_facility);
	_TRACE_LOG("level: %d", g_syslog_mgr.m_level);
	_TRACE_LOG("tag: %s", g_syslog_mgr.m_tag);
	_TRACE_LOG("hostname: %s", g_syslog_mgr.m_hostname);
	_TRACE_LOG("type: %#x", g_syslog_mgr.m_type);
	_TRACE_LOG("sock.domain: %d", g_syslog_mgr.m_sock.m_domain);
	_TRACE_LOG("sock.protocol: %d", g_syslog_mgr.m_sock.m_protocol);
	_TRACE_LOG("sock.localip: %s", g_syslog_mgr.m_sock.m_localip);
	_TRACE_LOG("sock.localport: %d", g_syslog_mgr.m_sock.m_localport);
	_TRACE_LOG("sock.serverip: %s", g_syslog_mgr.m_sock.m_serverip);
	_TRACE_LOG("sock.serverport: %d", g_syslog_mgr.m_sock.m_serverport);
	_TRACE_LOG("=================");
}


#if 1
#include <assert.h>

static void _cb(const char *msg, void *arg)
{
	(void)arg;
	MY_PRINTF("%s", msg);
}

int main()
{
	syslog_mgr_t* sm = NULL; 
	const char *str = "hello world";
	char buf[1024] = {0};
	int i = 0;
	
	syslog_mgr_init();
	syslog_mgr_dump();
	syslog_mgr_do(str);

	assert((sm = syslog_mgr()));
	sm->m_type |= SYSLOG_MGR_TYPE_NONLOCAL | SYSLOG_MGR_TYPE_PERROR;
	sm->m_facility = LOG_MAIL;
	sm->m_level = LOG_WARNING;
	sm->m_cb = _cb;
	syslog_mgr_dump();
	getchar();

	for (i = 0; i < 100; i++) {
		snprintf(buf, sizeof(buf), "%s:%d", str, i);
		syslog_mgr_do(buf);
	}

#ifdef _DEBUG
	syslog_mgr_level_atoi("a");
	MY_PRINTF("==========");
	syslog_mgr_facility_atoi("b");
#endif
	
	return 0;
}

#endif

