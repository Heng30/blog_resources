/* @desc:
 *  1. 对pipe的简单包装
 *  2. 通道的关闭都应该是写者发起的
 */

#ifndef _PIPE_MGR_H_
#define _PIPE_MGR_H_

#define PIPE_MGR_READER 0
#define PIPE_MGR_WRITER 1

typedef struct _pipe_mgr pipe_mgr_t;
typedef ssize_t (*pipe_mgr_reader_t) (pipe_mgr_t *pm, void *buf, size_t len);
typedef ssize_t (*pipe_mgr_writer_t) (pipe_mgr_t *pm, const void *buf, size_t len);
typedef void (*pipe_mgr_closer_t) (pipe_mgr_t *pm, unsigned char which);

typedef void* (*pipe_mgr_alloc_t) (size_t size);
typedef void (*pipe_mgr_free_t) (void *ptr);

struct _pipe_mgr {
    int m_pipe[2];
    pipe_mgr_reader_t m_reader;
    pipe_mgr_writer_t m_writer;
    pipe_mgr_closer_t m_closer;
};

/* @func:
 *  初始化管理器
 */
void pipe_mgr_init(pipe_mgr_alloc_t alloc, pipe_mgr_free_t dealloc);

/* @func:
 *  创建管理器
 */
pipe_mgr_t* pipe_mgr_new(void);

/* @func:
 *  打印信息
 */
void pipe_mgr_dump(pipe_mgr_t *pm);

#endif
