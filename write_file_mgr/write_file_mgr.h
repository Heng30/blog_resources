/* @fdesc：
 *	1. 写入文件，到达指定大小后会重写文件
 */

#ifndef _WRITE_FILE_MGR_H_
#define _WRITE_FILE_MGR_H_
#include <pthread.h>

typedef struct _write_file_mgr {
	char *m_dir; /* 文件保存的路径 */
	char *m_filename;	/* 文件名 */
	size_t m_rotate_size;	/* 重写文件大小 */
	pthread_mutex_t m_mutex;
} write_file_mgr_t;


/* @func:
 *	创建一个管理器
 * @param:
 *	dir: 必须以/结尾
 */
write_file_mgr_t* write_file_mgr_new(const char *dir, const char *filename, size_t rotate_size);

/* @func:
 *	销毁管理器
 */
void write_file_mgr_free(write_file_mgr_t *wm);

/* @func:
 *	写到文件中
 */
size_t write_file_mgr_do(write_file_mgr_t *wm, const char *buf, size_t buf_len);

/* @func:
 *	打印管理节点的信息
 */
void write_file_mgr_dump(write_file_mgr_t *wm);

#endif
