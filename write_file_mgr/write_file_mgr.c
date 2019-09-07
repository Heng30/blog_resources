#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "write_file_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define WRITE_FILE_MGR_TRACE_LOG MY_PRINTF
#define WRITE_FILE_MGR_DEBUG_LOG MY_PRINTF
#define WRITE_FILE_MGR_INFO_LOG MY_PRINTF
#define WRITE_FILE_MGR_WARN_LOG MY_PRINTF
#define WRITE_FILE_MGR_ERROR_LOG MY_PRINTF

typedef void* (*g_alloc_t) (size_t size);
typedef void (*g_free_t) (void *ptr);

static void* _malloc2calloc(size_t size);
static void _free(void *ptr);

static g_alloc_t g_wm_alloc = _malloc2calloc;
static g_free_t g_wm_free = _free;

static bool _is_dir_exsit(const char *dir)
{
	if (!dir) return false;
	struct stat st;
	if (lstat(dir, &st)) return false;
	return S_ISDIR(st.st_mode);
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
			WRITE_FILE_MGR_WARN_LOG("mkdir %s error, errno: %d - %s", dir_buf, errno, strerror(errno));
			return false;
		}
		
	next:
		*(start + 1) = ch; start++;
	}
	return true;
}

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

static void _free(void *ptr)
{
    if (!ptr) return ;
    free(ptr);
}

/* @func:
 *	创建一个管理器
 * @param:
 *	dir: 必须以/结尾
 */
write_file_mgr_t* write_file_mgr_new(const char *dir, const char *filename, size_t rotate_size)
{
	if (!dir || !filename || !rotate_size) return NULL;
	write_file_mgr_t *wm = NULL;
	size_t dir_len = strlen(dir);
	size_t filename_len = strlen(filename);
	
	if (!(wm = (write_file_mgr_t*)g_wm_alloc(sizeof(write_file_mgr_t)))) {
		WRITE_FILE_MGR_ERROR_LOG("g_wm_alloc error, errno: %d - %s", errno, strerror(errno));
		return NULL;
	}

	if (!(wm->m_dir = (char*)g_wm_alloc(dir_len + 1))) {
		WRITE_FILE_MGR_ERROR_LOG("g_wm_alloc error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

	if (!(wm->m_filename = (char*)g_wm_alloc(filename_len + 1))) {
		WRITE_FILE_MGR_ERROR_LOG("g_wm_alloc error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

	if (pthread_mutex_init(&wm->m_mutex, NULL)) {
		WRITE_FILE_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

	wm->m_rotate_size = rotate_size;
	memcpy(wm->m_dir, dir, dir_len); wm->m_dir[dir_len] = '\0';
	memcpy(wm->m_filename,  filename, filename_len); wm->m_filename[filename_len] = '\0';
	if (!_is_dir_exsit(dir)) _create_dir(dir, 0755);
	return wm;
	
err:
	if (wm) {
		if (wm->m_filename) g_wm_free(wm->m_filename);
		if (wm->m_dir) g_wm_free(wm->m_dir);
	}
	return NULL;
}

/* @func:
 *	销毁管理器
 */
void write_file_mgr_free(write_file_mgr_t *wm)
{
	if (!wm) return ;
	
	pthread_mutex_lock(&wm->m_mutex);
	g_wm_free(wm->m_dir);
	g_wm_free(wm->m_filename);
	pthread_mutex_destroy(&wm->m_mutex);
	g_wm_free(wm);
}

static size_t _write_file_mgr_do(write_file_mgr_t *wm, const char *buf, size_t buf_len)
{	
	FILE *fp = NULL;
	char path[PATH_MAX] = {0};
	struct stat st;
	size_t write_len = 0;
	
	snprintf(path, sizeof(path), "%s%s", wm->m_dir, wm->m_filename);
	
	if(lstat(path, &st) == -1){
		WRITE_FILE_MGR_WARN_LOG("stat %s error, errno: %d - %s", path, errno, strerror(errno));
		st.st_size = 0;
	}
	
	if((size_t)st.st_size >= wm->m_rotate_size) {
		WRITE_FILE_MGR_DEBUG_LOG("rotate %s, size: %ld", path, st.st_size);
		fp = fopen(path, "wb+");
	} else fp = fopen(path, "ab+");

	if (!fp) {
		WRITE_FILE_MGR_WARN_LOG("fopen %s errno, errno: %d - %s", path, errno, strerror(errno));
		return 0;
	}

	write_len = fwrite(buf, buf_len, 1, fp);
	fflush(fp); fclose(fp);
	
	return write_len;
}

/* @func:
 *	写到文件中
 */
size_t write_file_mgr_do(write_file_mgr_t *wm, const char *buf, size_t buf_len)
{
	if(!wm || !buf || !buf_len) return 0;
	size_t ret = 0;

	pthread_mutex_lock(&wm->m_mutex);
	ret = _write_file_mgr_do(wm, buf, buf_len);
	pthread_mutex_unlock(&wm->m_mutex);
	return ret;
}

/* @func:
 *	打印管理节点的信息
 */
void write_file_mgr_dump(write_file_mgr_t *wm)
{
	if (!wm) return ;

	pthread_mutex_lock(&wm->m_mutex);
	WRITE_FILE_MGR_TRACE_LOG("==============");
	WRITE_FILE_MGR_TRACE_LOG("dir: %s", wm->m_dir);
	WRITE_FILE_MGR_TRACE_LOG("filename: %s", wm->m_filename);
	WRITE_FILE_MGR_TRACE_LOG("rotate_size: %ld", wm->m_rotate_size);
	WRITE_FILE_MGR_TRACE_LOG("==============");
	pthread_mutex_unlock(&wm->m_mutex);
}

#if 1

#include <assert.h>

int main()
{
	write_file_mgr_t *wm = NULL;
	size_t i = 0;
	size_t max_num = 1024;
	pthread_t pt[10];

	assert((wm = write_file_mgr_new("/tmp/kk/", "a.dat", 10240)));
	write_file_mgr_dump(wm);

	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
		pthread_create(&pt[i], NULL, ({
			void* _(void *arg) {
				char buf[1024] = {0};
				ssize_t len = 0;
				size_t j = 0;
				for (j = 0; j < max_num; j++) {
					len = snprintf(buf, sizeof(buf), "%s - %ld\n", __FILE__, j);
					if (len <= 0) continue;
					write_file_mgr_do(wm, buf, len);
				}
				return arg;
			}; _;}), NULL);
	}

	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) 
		pthread_join(pt[i], NULL);

	write_file_mgr_free(wm);
	
	return 0;
}

#endif
