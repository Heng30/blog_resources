#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/types.h>

#include "log_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define LOG_MGR_TRACE_LOG MY_PRINTF
#define LOG_MGR_DEBUG_LOG MY_PRINTF
#define LOG_MGR_INFO_LOG MY_PRINTF
#define LOG_MGR_WARN_LOG MY_PRINTF
#define LOG_MGR_ERROR_LOG MY_PRINTF

typedef struct _log_mgr_t {
	char m_path[PATH_MAX];
	size_t m_rotate_size;
	unsigned char m_log_mask;
	bool m_is_stdout;
	pthread_mutex_t m_mutex;
} log_mgr_t;

log_mgr_t g_lm = { 
	.m_path = {0}, 
	.m_rotate_size = 1024 * 1024, 
	.m_log_mask = 63, 
	.m_is_stdout = false, 
};

static bool _is_dir_exsit(const char *dir)
{
	if (!dir) return false;
	struct stat st;
	if (lstat(dir, &st)) return false;
	return S_ISDIR(st.st_mode);
}

static bool _create_dir(const char *dir, mode_t mode)
{
	if (!dir) return false;
	char dir_buf[PATH_MAX] = {0};
	char *start = dir_buf;
	char ch = 0;

	if (!strchr(dir, '/')) return false;
	snprintf(dir_buf, sizeof(dir_buf), "%s/", dir);	
	
	while ((start = strchr(start, '/'))) {
		ch = *(start + 1);
		*(start + 1) = '\0';
		if (_is_dir_exsit(dir_buf)) goto next;
		if (-1 == mkdir(dir_buf, mode)) {
			LOG_MGR_WARN_LOG("mkdir %s error, errno: %d - %s", dir_buf, errno, strerror(errno));
			return false;
		}
		
	next:
		*(start + 1) = ch; start++;
	}
	return true;
}

static char _level_char_get(unsigned char level)
{
	switch (level) {
		case LOG_MGR_LV_TRACE: return 'T'; break;
		case LOG_MGR_LV_DEBUG: return 'D'; break;
		case LOG_MGR_LV_INFO: return 'I'; break;
		case LOG_MGR_LV_WARN: return 'W'; break;
		case LOG_MGR_LV_ERROR: return 'E'; break;
		case LOG_MGR_LV_FATAL: return 'F'; break;
	}
	return 'U';
}


/* @func:
 * 	初始化日志管理器
 */
bool log_mgr_init(const char *path, size_t rotate_size, unsigned char log_mask)
{
	if (!path) g_lm.m_is_stdout = true;
	char buf[PATH_MAX] = {0};
	char *ptr = NULL;
	size_t len = 0;
	
	if(pthread_mutex_init(&g_lm.m_mutex, NULL)) {
        LOG_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
        return false;
    }
	
	if (path) {
		snprintf(g_lm.m_path, sizeof(g_lm.m_path), "%s", path);
		if ((ptr = strrchr(path, '/'))) {
			len = (size_t)(ptr - path) > sizeof(buf) ? sizeof(buf) : (size_t)(ptr - path);
			memcpy(buf, path, len);
			if (!_is_dir_exsit(buf)) _create_dir(buf, 0755);
		}
	}
	
	g_lm.m_log_mask = log_mask;
	g_lm.m_rotate_size = rotate_size;
    return true;
}


/* @func:
 *	写日志文件
 */
void log_mgr_do(unsigned char level, const char *format, ...) 
{
    if (!(level & g_lm.m_log_mask)) return ;
	time_t nt = 0;
	struct tm ltm;
	char buf[64] = {0}; 
	FILE *fp = NULL;
	struct stat st;
	va_list vl;
	char level_c = 0;

	level_c = _level_char_get(level);
		
    pthread_mutex_lock(&g_lm.m_mutex); 
   	nt = time((time_t*)NULL); 
  	localtime_r(&nt, &ltm); 
    
    strftime(buf, sizeof(buf), "%b %d %T", &ltm); 
    if (g_lm.m_is_stdout) fp = stdout;
	else {
		if (lstat(g_lm.m_path, &st) == -1) {
			// LOG_MGR_WARN_LOG("lstat %s error, errno: %d - %s", g_lm.m_path, errno, strerror(errno));
			st.st_size = 0;
		}
		
		if (g_lm.m_rotate_size > (size_t)st.st_size) fp = fopen(g_lm.m_path, "ab+");
		else fp = fopen(g_lm.m_path, "wb");

		if (!fp) {
			LOG_MGR_WARN_LOG("fopen %s error, errno: %d - %s", g_lm.m_path, errno, strerror(errno));
			goto err;
		}
	}

	va_start(vl, format);
	fprintf(fp, "[%c] [%s] [%s] [%s:%d]: ", level_c, buf, __FILE__, __FUNCTION__, __LINE__);
	vfprintf(fp, format, vl); fprintf(fp, "\n"); fflush(fp);
	va_end(vl);
	if (!g_lm.m_is_stdout) fclose(fp); 
    
err:
    pthread_mutex_unlock(&g_lm.m_mutex); 
}


/* @func:
 *		打印调试日志信息
 */
void log_mgr_dump(void)
{	
    LOG_MGR_TRACE_LOG("==================");
	LOG_MGR_TRACE_LOG("path: %s", g_lm.m_path);
    LOG_MGR_TRACE_LOG("rotate_size: %lu", g_lm.m_rotate_size);; 
    LOG_MGR_TRACE_LOG("log_mask: %u", g_lm.m_log_mask);
	LOG_MGR_TRACE_LOG("is_stdout: %d", g_lm.m_is_stdout);
    LOG_MGR_TRACE_LOG("==================");
}

#if 1

#include <assert.h>

int main()
{
	size_t i = 0;
	size_t max_num = 10240;
	pthread_t pt[10];

	assert(log_mgr_init("/tmp/kk/test.dat", 1024 * 1024, LOG_MGR_LV_WARN | LOG_MGR_LV_DEBUG));
	log_mgr_dump();

	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
		pthread_create(&pt[i], NULL, ({
			void* _(void *arg) {
				char *str = "hello";
				size_t j = 0;
				for (j = 0; j < max_num; j++) {
					log_mgr_trace("%s:%d", str, j);
					log_mgr_debug("%s:%d", str, j);
					log_mgr_info("%s:%d", str, j);
					log_mgr_warn("%s:%d", str, j);
					log_mgr_error("%s:%d", str, j);
					log_mgr_fatal("%s:%d", str, j);
				}
				return arg;
			}; _;}), NULL);
	}

	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) 
		pthread_join(pt[i], NULL);

	MY_PRINTF("OK");

	return 0;
}

#endif

