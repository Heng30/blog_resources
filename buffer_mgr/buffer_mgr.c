#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "buffer_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define BUFFER_MGR_TRACE_LOG MY_PRINTF
#define BUFFER_MGR_DEBUG_LOG MY_PRINTF
#define BUFFER_MGR_INFO_LOG MY_PRINTF
#define BUFFER_MGR_WARN_LOG MY_PRINTF
#define BUFFER_MGR_ERROR_LOG MY_PRINTF

#define BUFFER_MGR_INIT_CAPACITY 1024
#define BUFFER_MGR_SIZEOF (sizeof(buffer_mgr_t))

static buffer_mgr_alloc_t g_bm_alloc = NULL;
static buffer_mgr_free_t g_bm_free = NULL;

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

void buffer_mgr_dump(buffer_mgr_t *bm)
{
	if (!bm) return ;

	pthread_mutex_lock(&bm->m_mutex);
	BUFFER_MGR_TRACE_LOG("=================");
	BUFFER_MGR_TRACE_LOG("start: %lu", bm->m_start);
	BUFFER_MGR_TRACE_LOG("end: %lu", bm->m_end);
	BUFFER_MGR_TRACE_LOG("reserved: %lu", bm->m_reserved);
	BUFFER_MGR_TRACE_LOG("rd_offset: %lu", bm->m_rd_offset);
	BUFFER_MGR_TRACE_LOG("wr_offset: %lu", bm->m_wr_offset);
	BUFFER_MGR_TRACE_LOG("=================");
	pthread_mutex_unlock(&bm->m_mutex);
}

/* @func:
 *	初始化管理器
 */
void buffer_mgr_init(buffer_mgr_alloc_t alloc, buffer_mgr_free_t dealloc)
{
	if (!alloc || !dealloc) g_bm_alloc = _malloc2calloc, g_bm_free = free;
	else g_bm_alloc = alloc, g_bm_free = dealloc;
}

/* @func:
 *	创建一个管理器
 */
buffer_mgr_t* buffer_mgr_new(size_t capacity, size_t reserved)
{
	if (!capacity) capacity = BUFFER_MGR_INIT_CAPACITY;
	buffer_mgr_t *bm = NULL;

	if (!(bm = g_bm_alloc(BUFFER_MGR_SIZEOF + capacity + reserved + 1))) {
		BUFFER_MGR_ERROR_LOG("g_bm_alloc error, errno: %d - %s", errno, strerror(errno));
		return NULL;
	}

	if (pthread_mutex_init(&bm->m_mutex, NULL)) {
		BUFFER_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

	bm->m_reserved = BUFFER_MGR_SIZEOF;
	bm->m_start = bm->m_reserved + reserved;
	bm->m_rd_offset = bm->m_wr_offset = bm->m_start;
	bm->m_end = bm->m_start + capacity;
	return bm;

err:
	if (bm) free(bm);
	return NULL;
}


/* @func:
 *	销毁一个管理器
 */
void buffer_mgr_free(buffer_mgr_t *bm)
{
	if (!bm) return ;
	pthread_mutex_lock(&bm->m_mutex);
	g_bm_free(bm);
	pthread_mutex_destroy(&bm->m_mutex);
}

/* @func:
 *	读取套接字内容
 */
static ssize_t _read_all(int fd, void *buf, size_t len)
{
	if (fd < 0 || !buf || !len) return 0;
	ssize_t byte = 0;

	do {
		byte = read(fd, buf, len);
	} while (byte < 0 && errno == EINTR);

	if (byte < 0) {
		BUFFER_MGR_WARN_LOG("write error, errno: %d - %s", errno, strerror(errno));
	}
	return byte;
}

/* @func:
 *	移动数据到start处
 */
static void _buffer_mgr_move(buffer_mgr_t *bm)
{
	if (!bm) return ;
	size_t offset = 0;

	if (bm->m_wr_offset <= bm->m_rd_offset) {
		bm->m_rd_offset = bm->m_wr_offset = bm->m_start;
		return ;
	}

	offset = bm->m_wr_offset - bm->m_rd_offset;
	memmove((void*)bm + bm->m_start, (void*)bm + bm->m_rd_offset, offset);
	bm->m_rd_offset = bm->m_start;
	bm->m_wr_offset = bm->m_start + offset;
}


/* @func:
 *	从套接字中读取内容
 */
ssize_t buffer_mgr_read(buffer_mgr_t *bm, int fd)
{
	if (!bm || fd < 0) return -1;
	ssize_t byte = 0;
	size_t count = 0;

	pthread_mutex_lock(&bm->m_mutex);
	if (bm->m_wr_offset >= bm->m_end) _buffer_mgr_move(bm);
	count = bm->m_end - bm->m_wr_offset;
	byte = _read_all(fd,  (void*)bm +  bm->m_wr_offset, count);
	if (byte > 0) bm->m_wr_offset += byte;
	*((char*)bm + bm->m_wr_offset) = '\0';
	pthread_mutex_unlock(&bm->m_mutex);

	return byte;
}

/* @func:
 *	获取当前可读数据的副本
 */
void* buffer_mgr_copy_new(buffer_mgr_t *bm, size_t *len)
{
	if (!bm || !len) return NULL;
	void *ptr = NULL;
	size_t offset = 0;
	*len = 0;

	pthread_mutex_lock(&bm->m_mutex);
	if (bm->m_wr_offset > bm->m_rd_offset) {
		offset = bm->m_wr_offset - bm->m_rd_offset;
		if((ptr = g_bm_alloc(offset + 1))) {
			memcpy(ptr, (void*)bm + bm->m_rd_offset, offset);
			*((char*)ptr + offset) = '\0';
			*len = offset;
		}
	}
	pthread_mutex_unlock(&bm->m_mutex);

	return ptr;
}

/* @func:
 *	释放副本占用的内存
 */
void buffer_mgr_copy_free(void *ptr)
{
	if (!ptr) return ;
	g_bm_free(ptr);
}

/* @func:
 *	重置读写的偏移位置
 */
void buffer_mgr_reset(buffer_mgr_t *bm)
{
	if (!bm) return ;
	pthread_mutex_lock(&bm->m_mutex);
	bm->m_rd_offset = bm->m_wr_offset = bm->m_start;
	pthread_mutex_unlock(&bm->m_mutex);
}

/* @func:
 *	写入套接字
 */
static ssize_t _write_all(int fd, const void *buf, size_t len)
{
	if (fd < 0 || !buf || !len) return 0;
	size_t total = 0;
	ssize_t writelen = 0;

	while (len > 0) {
		writelen = write(fd, buf, len);
		if (writelen < 0) {
			if (errno == EINTR) continue;
			BUFFER_MGR_WARN_LOG("write error, errno: %d - %s", errno, strerror(errno));
			return writelen;
		}

		total += writelen;
		buf += writelen;
		len -= writelen;
	}
	return total;
}

/* @func:
 *	将缓冲区的数据写入套接字
 */
ssize_t buffer_mgr_write(buffer_mgr_t *bm, int fd)
{
	if (fd < 0 || !bm) return -1;
	ssize_t byte = 0;
	size_t count = 0;

	pthread_mutex_lock(&bm->m_mutex);
	if (bm->m_wr_offset > bm->m_rd_offset) {
		count =  bm->m_wr_offset - bm->m_rd_offset;
		if (0 < (byte = _write_all(fd,  (void*)bm +  bm->m_rd_offset, count))) {
			bm->m_rd_offset += byte;
			if (bm->m_rd_offset >= bm->m_wr_offset) bm->m_rd_offset = bm->m_wr_offset = bm->m_start;
		}
	}
	pthread_mutex_unlock(&bm->m_mutex);

	return byte;
}

/* @func:
 *	对缓冲区的数据进行操作，
 * @warn:
 *	不能对缓冲区的数据进行修改, 因为修改后数据的大小可能发生变化，但管理节点的信息没有更新
 */
void* buffer_mgr_filter(buffer_mgr_t *bm, buffer_mgr_filter_func_t filter_func, void *arg)
{
	if (!bm || !filter_func) return NULL;
	void *ret = NULL;
	size_t offset = 0;

	pthread_mutex_lock(&bm->m_mutex);
	if (bm->m_wr_offset > bm->m_rd_offset) {
		offset = bm->m_wr_offset - bm->m_rd_offset;
		ret = filter_func((void*)bm + bm->m_rd_offset, offset, arg);
	}
	pthread_mutex_unlock(&bm->m_mutex);

	return ret;
}

/* @func:
 *	更新读取位置
 */
void buffer_mgr_update(buffer_mgr_t *bm, buffer_mgr_update_func_t update_func, void *arg)
{
	if (!bm || !update_func) return ;
	size_t offset = 0, len = 0;

	pthread_mutex_lock(&bm->m_mutex);
	if (bm->m_wr_offset > bm->m_rd_offset) {
		offset = bm->m_wr_offset - bm->m_rd_offset;
		len = update_func((void*)bm + bm->m_rd_offset, offset, arg);
		bm->m_rd_offset += len;
		if (bm->m_rd_offset >= bm->m_wr_offset) bm->m_rd_offset = bm->m_wr_offset = bm->m_start;
	}
	pthread_mutex_unlock(&bm->m_mutex);
}

/* @func:
 * 	设置reserved的内存
 */
bool buffer_mgr_reserved_set(buffer_mgr_t *bm, void *reserved, size_t len)
{
	if (!bm || !reserved) return false;
	if (!bm->m_reserved) return false;
	if (bm->m_start <= bm->m_reserved) return false;
	size_t offset = bm->m_start - bm->m_reserved;
	if (offset < len) return false;

	pthread_mutex_lock(&bm->m_mutex);
	memcpy((void*)bm + bm->m_reserved, reserved, len);
	pthread_mutex_unlock(&bm->m_mutex);
	return true;
}

#include <assert.h>
static size_t g_count_1 = 0;
static size_t g_count_2 = 0;

static void* _filter_func(const void *ptr, size_t len, void *arg)
{
	if (!ptr || !len) return NULL;
	char *ret_1 = NULL, *ret_2 = NULL;
	if ((ret_1 = strstr(ptr, "_lock"))) g_count_1++;
	if ((ret_2 = strstr(ptr, "_unlock"))) g_count_2++;
	return arg;
}

static size_t _update_func(const void *ptr, size_t len, void *arg)
{
	if (!ptr || !len) return 0;
	void *ret = NULL;
	if ((ret = strstr(ptr, "lo"))) return ret - ptr;
	return 0;
}

int main()
{
	const char *file = "./1_data.txt";
	const char *file_2 = "./2_data.txt";
	const char *file_3 = "./3_data.txt";
    const char *str = "hello world";
	void *ptr = NULL;
	size_t len = 0, i = 0;
	pthread_t pt[30];
    mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;

	int fd = open(file, O_CREAT|O_RDWR, mode);
	int fd_2 = open(file_2, O_CREAT|O_RDWR, mode);
	int fd_3 = open(file_3, O_CREAT|O_RDWR, mode);

	assert(fd >= 0 && fd_2 >= 0 && fd_3 >= 0);
    write(fd, str, strlen(str));
    close(fd);
    assert((fd = open(file, O_RDONLY, mode)) >= 0);

	buffer_mgr_init(NULL, NULL);
	buffer_mgr_t *bm = NULL;
	assert((bm = buffer_mgr_new(10240, 10)));
	assert(buffer_mgr_reserved_set(bm, "reserved", strlen("reserved")));
	MY_PRINTF("reserved: %s", (char*)bm + bm->m_reserved);
	buffer_mgr_dump(bm);

	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
		pthread_create(&pt[i], NULL, ({
			void* _(void *arg) {
				while (0 < buffer_mgr_read(bm, fd)) {
					assert((ptr = buffer_mgr_copy_new(bm, &len)));
					write(fd_3, ptr, len);
					buffer_mgr_copy_free(ptr);
					buffer_mgr_filter(bm, _filter_func, NULL);
					buffer_mgr_update(bm, _update_func, NULL);
					buffer_mgr_write(bm, fd_2);
				}
				return arg;
			}; _;}), NULL);
	}

	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++)
		pthread_join(pt[i], NULL);

	buffer_mgr_dump(bm);
	buffer_mgr_reset(bm);
	buffer_mgr_free(bm);
	MY_PRINTF("g_count_1: %lu", g_count_1);
	MY_PRINTF("g_count_2: %lu", g_count_2);

	close(fd), close(fd_2), close(fd_3);
	return 0;
}
