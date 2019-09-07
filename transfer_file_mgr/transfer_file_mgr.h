/* @desc:
 *	1. 写语句到文件中，到达指定的行数对文件进行转移
 */
#ifndef _TRANSFER_FILE_MGR_H_
#define _TRANSFER_FILE_MGR_H_

typedef struct _transfer_file_mgr {
	char *m_tmp_dir; /* 临时目录 */
	char *m_dst_dir; /* 最终转移的目录 */
	char *m_filename; /* 文件名 */
	char *m_suffix;	 /* 文件名后缀 */
	size_t m_max_line; /* 最大行数 */
	size_t m_write_line; /* 当前写入的行数 */
	pthread_mutex_t m_mutex;
} transfer_file_mgr_t;

transfer_file_mgr_t* transfer_file_mgr_new(const char *tmp_dir, const char *dst_dir, 
                            const char *filename, const char *suffix, size_t max_line);
/* @func:
 *	销毁管理器
 */
void transfer_file_mgr_free(transfer_file_mgr_t *tm);

/* @func:
 *	写日志
 */
ssize_t transfer_file_mgr_printf(transfer_file_mgr_t *tm, const char *fm, ...);


/* @func:
 *	转移日志文件, 
 */
void transfer_file_mgr_do(transfer_file_mgr_t* tm);

#endif
