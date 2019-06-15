#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>

#include "pipe_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define PIPE_MGR_WARN_LOG MY_PRINTF
#define PIPE_MGR_DEBUG_LOG MY_PRINTF
#define PIPE_MGR_ERROR_LOG MY_PRINTF
#define PIPE_MGR_INFO_LOG MY_PRINTF
#define PIPE_MGR_TRACE_LOG MY_PRINTF

static pipe_mgr_alloc_t g_pm_alloc = NULL;
static pipe_mgr_free_t g_pm_free = NULL;

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

static ssize_t _pipe_mgr_reader(pipe_mgr_t *pm, void *buf, size_t len)
{
	if (!len || !buf || !pm) return -1;
	ssize_t bytes = 0;
    int fd = 0;

    if ((fd = pm->m_pipe[PIPE_MGR_READER]) < 0) return -1;

	do {
		bytes = read(fd, buf, len);
	} while (bytes < 0 && errno == EINTR);

	return bytes;
}

static ssize_t _pipe_mgr_writer(pipe_mgr_t *pm, const void* buf, size_t len)
{
    if (!pm || !buf || !len) return -1;
    size_t total = 0;
	ssize_t written = 0;
    int fd = 0;

    if ((fd = pm->m_pipe[PIPE_MGR_WRITER]) < 0) return -1;
	while (len > 0) {
		if ((written = write(fd, buf, len)) < 0) {
			if (errno == EINTR) { errno = 0; continue; }
			return written;
		}

		total += written; buf += written; len -= written;
	}
	return total;
}

/* @func:
 *  关闭通道
 */
static void _pipe_mgr_closer(pipe_mgr_t *pm, unsigned char which)
{
    if (!pm) return ;

    if ((which == PIPE_MGR_READER) && (pm->m_pipe[PIPE_MGR_READER] >= 0)) {
        close(pm->m_pipe[PIPE_MGR_READER]);
        pm->m_pipe[PIPE_MGR_READER] = -1;
    }

    if ((which == PIPE_MGR_WRITER) && (pm->m_pipe[PIPE_MGR_WRITER] >= 0)) {
        close(pm->m_pipe[PIPE_MGR_WRITER]);
        pm->m_pipe[PIPE_MGR_WRITER] = -1;
    }

    if (pm->m_pipe[PIPE_MGR_READER] < 0 && pm->m_pipe[PIPE_MGR_WRITER] < 0)
        g_pm_free(pm);
}


/* @func:
 *  初始化管理器
 */
void pipe_mgr_init(pipe_mgr_alloc_t alloc, pipe_mgr_free_t dealloc)
{
    if (!alloc || !dealloc) g_pm_alloc = _malloc2calloc, g_pm_free = free;
    else g_pm_alloc = alloc, g_pm_free = dealloc;
}

/* @func:
 *  创建管理器
 */
pipe_mgr_t* pipe_mgr_new(void)
{
    pipe_mgr_t *pm = NULL;

    if (!(pm = g_pm_alloc(sizeof(pipe_mgr_t)))) {
        PIPE_MGR_ERROR_LOG("g_pm_alloc error, errno: %d - %s", errno, strerror(errno));
        return NULL;
    }

    if (pipe(pm->m_pipe) == -1) {
        PIPE_MGR_ERROR_LOG("pipe error, errno: %d - %s", errno, strerror(errno));
        goto err;
    }

    pm->m_reader = _pipe_mgr_reader;
    pm->m_writer = _pipe_mgr_writer;
    pm->m_closer = _pipe_mgr_closer;
    return pm;

err:
    if (!pm) g_pm_free(pm);
    return NULL;
}

/* @func:
 *  打印信息
 */
void pipe_mgr_dump(pipe_mgr_t *pm)
{
    if (!pm) return ;

    PIPE_MGR_TRACE_LOG("==========");
    PIPE_MGR_TRACE_LOG("pipe: %d - %d", pm->m_pipe[0], pm->m_pipe[1]);
    PIPE_MGR_TRACE_LOG("reader: %p", pm->m_reader);
    PIPE_MGR_TRACE_LOG("writer: %p", pm->m_writer);
    PIPE_MGR_TRACE_LOG("closer: %p", pm->m_closer);
    PIPE_MGR_TRACE_LOG("==========");
}

#if 1
#include <assert.h>
#define MAX_NUM 1024

struct _test {
    int m_num;
};

int main()
{
	pthread_t pt[2];
	size_t i = 0;
    pipe_mgr_t *pm = NULL;
    pipe_mgr_init(NULL, NULL);

	assert((pm = pipe_mgr_new()));
	pipe_mgr_dump(pm); 

	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
		pthread_create(&pt[i], NULL, ({
			void* _(void *arg) {
                int pt_index = (int)arg;
                struct _test t;
                int j = 0;
                int ret = 0;

                if (pt_index == 0) {
                    for (j = 0; j < MAX_NUM; j++) {
                        t.m_num = j;
                        ret = pm->m_writer(pm, &t, sizeof(t));
                    }
                    pm->m_closer(pm, PIPE_MGR_WRITER);
                } else {
                    while ((ret = pm->m_reader(pm, &t, sizeof(t)) > 0)) {
                        assert(j == t.m_num);
                        j++;
                    }

                    assert(j >= MAX_NUM);
                    pm->m_closer(pm, PIPE_MGR_READER);
                }

				return (void*)ret;
			}; _;}), (void*)i);
	}


	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++)
		pthread_join(pt[i], NULL);

    MY_PRINTF("OK");
    return 0;
}

#endif
