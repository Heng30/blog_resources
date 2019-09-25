/* @desc:
 *      调试日志框架，支持stdout输出和输出到日志文件中;
 */
#ifndef _LOG_MGR_H_
#define _LOG_MGR_H_

#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#define LOG_MGR_LV_TRACE 1
#define LOG_MGR_LV_DEBUG 2
#define LOG_MGR_LV_INFO  4
#define LOG_MGR_LV_WARN  8
#define LOG_MGR_LV_ERROR 16
#define LOG_MGR_LV_FATAL 32
#define LOG_MGR_LV_ALL 63

typedef struct _log_mgr_t {
	char m_path[PATH_MAX];
	size_t m_rotate_size;
	unsigned char m_log_mask;
	bool m_is_stdout;
	pthread_mutex_t m_mutex;
} log_mgr_t;

/* @func:
 * 	初始化日志管理器
 */
void log_mgr_init(void);

/* @func:
 *	写日志文件
 */
void log_mgr_do(unsigned char level, const char *file, const char *func, int line, const char *format, ...);

#define log_mgr_trace(format, ...) log_mgr_do(LOG_MGR_LV_TRACE, __FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define log_mgr_debug(format, ...) log_mgr_do(LOG_MGR_LV_DEBUG, __FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define log_mgr_info(format, ...)  log_mgr_do(LOG_MGR_LV_INFO , __FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define log_mgr_warn(format, ...)  log_mgr_do(LOG_MGR_LV_WARN , __FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define log_mgr_error(format, ...) log_mgr_do(LOG_MGR_LV_ERROR, __FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define log_mgr_fatal(format, ...) log_mgr_do(LOG_MGR_LV_FATAL, __FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

#endif
