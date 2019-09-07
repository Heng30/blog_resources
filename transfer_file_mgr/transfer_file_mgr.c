#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>


#include "transfer_file_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define TRANSFER_FILE_MGR_WARN_LOG MY_PRINTF
#define TRANSFER_FILE_MGR_DEBUG_LOG MY_PRINTF
#define TRANSFER_FILE_MGR_ERROR_LOG MY_PRINTF
#define TRANSFER_FILE_MGR_INFO_LOG MY_PRINTF
#define TRANSFER_FILE_MGR_TRACE_LOG MY_PRINTF


typedef void* (*g_alloc_t) (size_t size);
typedef void (*g_free_t) (void *ptr);

static void* _malloc2calloc(size_t size);
static void _free(void *ptr);
static g_alloc_t g_tm_alloc = _malloc2calloc;
static g_free_t g_tm_free = _free;

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

static void _free(void *ptr)
{
    if (!ptr) return ;
    free(ptr);
}

static bool _is_dir_exsit(const char *dir)
{
	if (!dir) return false;
	struct stat st;
	if (lstat(dir, &st)) return false;
	return S_ISDIR(st.st_mode);
}

static bool _is_file_exsit(const char *path)
{
	if (!path) return false;
	struct stat st;
	if (lstat(path, &st)) return false;
	return S_ISREG(st.st_mode);
}


/* @func:
 *	创建一个目录
 */
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
			TRANSFER_FILE_MGR_WARN_LOG("mkdir %s error, errno: %d - %s", dir_buf, errno, strerror(errno));
			return false;
		}
		
	next:
		*(start + 1) = ch; start++;
	}
	return true;
}


/* @func:
 *	获取文件名
 */
static void _filename_get(transfer_file_mgr_t *tm, char *buf, size_t buf_size)
{
	if (!tm || !tm->m_dst_dir || !tm->m_filename || !buf || !buf_size) return ;
	struct tm tm_s;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm_s);
	
	snprintf(buf, buf_size, "%s%s-%04d%02d%02d%02d%02d%02d%03d%s",
				tm->m_dst_dir, tm->m_filename, 1900 + tm_s.tm_year,
				1 + tm_s.tm_mon, tm_s.tm_mday, tm_s.tm_hour, tm_s.tm_min,
				tm_s.tm_sec, (int)tv.tv_usec, tm->m_suffix);	
}

/* @func:
 *  转移文件
 */
static void _transfer_file_mgr_do(transfer_file_mgr_t *tm)
{
	if (!tm) return ;
	char path_dst[PATH_MAX] = {0};
	char path_tmp[PATH_MAX] = {0};

	snprintf(path_tmp, sizeof(path_tmp), "%s%s%s", tm->m_tmp_dir, tm->m_filename, tm->m_suffix);
	if (tm->m_write_line == 0) return ;
	if (!_is_file_exsit(path_tmp)) return ;
	
	_filename_get(tm, path_dst, sizeof(path_dst));
	if (rename(path_tmp, path_dst) < 0) {
		TRANSFER_FILE_MGR_WARN_LOG("rename %s to %s error, errno: %d - %s", path_tmp, path_dst, errno, strerror(errno));
		return ;
	} 
	tm->m_write_line = 0;
}

/* @func:
 *	创建一个管理器
 * @param:
 *	dir: 必须以/结尾
 */
transfer_file_mgr_t* transfer_file_mgr_new(const char *tmp_dir, const char *dst_dir, const char *filename, const char *suffix, size_t max_line)
{
	if (!tmp_dir || !dst_dir || !filename || !suffix || !max_line) return NULL;
	transfer_file_mgr_t *tm = NULL;
	size_t tmp_dir_len = strlen(tmp_dir);
	size_t dst_dir_len = strlen(dst_dir);
	size_t filename_len = strlen(filename);
	size_t suffix_len = strlen(suffix);
	
	if (!(tm = (transfer_file_mgr_t*)g_tm_alloc(sizeof(transfer_file_mgr_t)))) {
		TRANSFER_FILE_MGR_ERROR_LOG("g_tm_alloc error, errno: %d - %s", errno, strerror(errno));
		return NULL;
	}

	if (!(tm->m_tmp_dir = (char*)g_tm_alloc(tmp_dir_len + 1))) {
		TRANSFER_FILE_MGR_ERROR_LOG("g_tm_alloc error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

	if (!(tm->m_dst_dir = (char*)g_tm_alloc(dst_dir_len + 1))) {
		TRANSFER_FILE_MGR_ERROR_LOG("g_tm_alloc error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

	if (!(tm->m_filename = (char*)g_tm_alloc(filename_len + 1))) {
		TRANSFER_FILE_MGR_ERROR_LOG("g_tm_alloc error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

	if (!(tm->m_suffix = (char*)g_tm_alloc(suffix_len + 1))) {
		TRANSFER_FILE_MGR_ERROR_LOG("g_tm_alloc error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

	if (pthread_mutex_init(&tm->m_mutex, NULL)) {
		TRANSFER_FILE_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

	tm->m_max_line = max_line;
	memcpy(tm->m_tmp_dir, tmp_dir, tmp_dir_len); tm->m_tmp_dir[tmp_dir_len] = '\0';
	memcpy(tm->m_dst_dir, dst_dir, dst_dir_len); tm->m_dst_dir[dst_dir_len] = '\0';
	memcpy(tm->m_suffix,  suffix, suffix_len); tm->m_suffix[suffix_len] = '\0';
	memcpy(tm->m_filename,  filename, filename_len); tm->m_filename[filename_len] = '\0';
	if (!_is_dir_exsit(tmp_dir)) _create_dir(tmp_dir, 0755);
	if (!_is_dir_exsit(dst_dir)) _create_dir(dst_dir, 0755);
	return tm;
	
err:
	if (tm) {
		if (tm->m_filename) g_tm_free(tm->m_filename);
		if (tm->m_tmp_dir) g_tm_free(tm->m_tmp_dir);
		if (tm->m_dst_dir) g_tm_free(tm->m_dst_dir);
		if (tm->m_suffix) g_tm_free(tm->m_suffix);
	}
	return NULL;
}


/* @func:
 *	销毁管理器
 */
void transfer_file_mgr_free(transfer_file_mgr_t *tm)
{
	if (!tm) return ;
	
	pthread_mutex_lock(&tm->m_mutex);
	g_tm_free(tm->m_filename);
	g_tm_free(tm->m_dst_dir);
	g_tm_free(tm->m_tmp_dir);
	g_tm_free(tm->m_suffix);
	pthread_mutex_destroy(&tm->m_mutex);
	g_tm_free(tm);
}

/* @func:
 *	写日志
 */
ssize_t transfer_file_mgr_printf(transfer_file_mgr_t *tm, const char *fm, ...)
{
	if (!tm) return -1;
    va_list ap;
	FILE *fp = NULL;
	char path[PATH_MAX] = {0};
	int write_len = 0;

	pthread_mutex_lock(&tm->m_mutex);
	snprintf(path, sizeof(path), "%s%s%s", tm->m_tmp_dir, tm->m_filename, tm->m_suffix);
	if (!(fp = fopen(path, "ab+"))) {
		TRANSFER_FILE_MGR_WARN_LOG("fopen %s error, errno: %d - %s", path, errno, strerror(errno));
		goto out;
	}
	
	va_start(ap, fm); write_len = vfprintf(fp, fm, ap); va_end(ap);
	if (write_len < 0) {
		write_len = -1; goto out;
	} else tm->m_write_line++;
	
out:
	if (fp) { fflush(fp); fclose(fp); }
	if (tm->m_write_line >= tm->m_max_line) _transfer_file_mgr_do(tm);
	pthread_mutex_unlock(&tm->m_mutex);
	return write_len;

}

/* @func:
 *	转移日志文件, 
 */
void transfer_file_mgr_do(transfer_file_mgr_t* tm)
{
	if (!tm) return ;
	
	pthread_mutex_lock(&tm->m_mutex);
	_transfer_file_mgr_do(tm);
	pthread_mutex_unlock(&tm->m_mutex);
}

#if 1
#include <assert.h>

int main()
{
	size_t i = 0;
    int cnt = 0;
	transfer_file_mgr_t *tm = NULL;
	pthread_t pt[10];

	assert((tm = transfer_file_mgr_new("/tmp/", "/tmp/f/", "test", ".dat", 70)));


	pthread_create(&pt[i], NULL, ({
		void* _(void *arg) {
			while (true) {
				transfer_file_mgr_do(tm);
                if (cnt == sizeof(pt) / sizeof(pt[0]) - 1) break;
				sleep(1);
			}
			
			return arg;
	}; _;}), NULL);
	
	for (i = 1; i < sizeof(pt) / sizeof(pt[0]); i++) {
		pthread_create(&pt[i], NULL, ({
			void* _(void *arg) {
				int j = 0;
				int line = 100;
				for (j = 0; j < line; j++) {
					transfer_file_mgr_printf(tm, "%s:%d - %d\n", __FILE__, __LINE__, j);
					/* usleep(1000 * 100); */
				}
				
                cnt++;
				return arg;
		}; _;}), NULL);
	}

	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) 
		pthread_join(pt[i], NULL);

	transfer_file_mgr_free(tm);
	return 0;
}

#endif

