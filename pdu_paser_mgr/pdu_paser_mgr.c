#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#include "pdu_paser_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define _TRACE_LOG MY_PRINTF
#define _DEBUG_LOG MY_PRINTF
#define _INFO_LOG MY_PRINTF
#define _WARN_LOG MY_PRINTF
#define _ERROR_LOG MY_PRINTF

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define CMD_LINE_LEN 4096

typedef void* (*g_alloc_t) (size_t size);
typedef void (*g_free_t) (void *ptr);

static void* _malloc2calloc(size_t size);
static void _free(void *ptr);
static g_alloc_t g_alloc = _malloc2calloc;
static g_free_t g_free = _free;

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

static void _free(void *ptr)
{
	if (!ptr) return;
	free(ptr);
}

/* @func:
 *	创建一个管理节
 */
static pdu_paser_mgr_t* _pdu_paser_mgr_new(void)
{
	pdu_paser_mgr_t *mgr = NULL;
	if (!(mgr = (pdu_paser_mgr_t*)g_alloc(sizeof(pdu_paser_mgr_t)))) {
		_WARN_LOG("calloc error, errno: %d - %s", errno, strerror(errno));
		return NULL;
	}
	return mgr;
}

/* @func:
 *	解析pdu数据包，并产生一个管理节点链
 * 
 * @param:
 *	pdu_start: 开头数据的位置
 *	pud_end: 结尾数据的后一个位
 *	end_pos: 每一个数据的分割
 */
pdu_paser_mgr_t* pdu_paser_mgr_paser(const char *pdu_start, const char *pdu_end, const char *end_pos)
{
	if (!pdu_start || !pdu_start || !end_pos) return NULL;
	const char *ptr_s = pdu_start, *ptr_e = NULL;
	pdu_paser_mgr_t *head = NULL, *tail = NULL, *item = NULL;
	bool is_end = false;
	
	while (ptr_s < pdu_end) {
		if (!(item = _pdu_paser_mgr_new())) goto err;
		if (!(ptr_e = strstr(ptr_s, end_pos))) {
			item->m_old_start = ptr_s;
			item->m_old_end = pdu_end;
			is_end = true;
		} else {
			item->m_old_start = ptr_s;
			item->m_old_end = ptr_e + strlen(end_pos);
			ptr_s = item->m_old_end;
		}
		
		if (!head || !tail) head = tail = item;
		else tail->m_next = item, tail = item;

		if (is_end) break;
	}

	return head;

err:
	for ( ; head; head = tail) {
		tail = head->m_next;
		g_free(head);
	}
	return NULL;
}

/* @func:
 *	释放占用的内存
 */
void pdu_paser_mgr_free(pdu_paser_mgr_t **hdr)
{
	if (!hdr) return ;
	pdu_paser_mgr_t *item = NULL, *head = *hdr;
	
	for ( ; head; head = item) {
		item = head->m_next;
		if (head->m_new) g_free((void*)head->m_new);
		g_free((void*)head);
	}
	*hdr = (pdu_paser_mgr_t*)NULL;
}

/* @func:
 *	打印信息
 */
void pdu_paser_mgr_dump(pdu_paser_mgr_t *head)
{
	if (!head) return ;
	char buf[CMD_LINE_LEN];
	size_t offset = 0; 

	for ( ; head; head = head->m_next) {
		offset = head->m_old_end > head->m_old_start ? head->m_old_end - head->m_old_start : 0;
		strncpy(buf, head->m_old_start, MIN(offset, sizeof(buf) - 1));
		buf[MIN(offset, sizeof(buf) - 1)] = '\0';
		_TRACE_LOG("===================");
		_TRACE_LOG("old_str: %s", buf);
		_TRACE_LOG("new_str: %s", head->m_new ? head->m_new : "");
		_TRACE_LOG("===================");
	}
}

/* @func:
 *	重组pdu数据
 */
const char* pdu_paser_mgr_join(pdu_paser_mgr_t *head)
{
	if (!head) return NULL;
	bool is_join = false;
	size_t len = 0, offset = 0;
	char *buf = NULL, *buf_start = NULL;
	pdu_paser_mgr_t *item = NULL;
	
	for (item = head; item; item = item->m_next) {
		if (item->m_new){
			is_join = true;
			len += strlen(item->m_new);
		} else {
			offset = item->m_old_end > item->m_old_start ? item->m_old_end - item->m_old_start : 0;
			len += offset;
		}
	}
	
	if (!is_join) return NULL;
	if (!(buf = (char*)g_alloc(len + 1))) {
		_WARN_LOG("g_alloc error, errno: %d - %s", errno, strerror(errno));
		return NULL;
	}
	buf_start = buf;
	
	for (item = head; item; item = item->m_next) {
		if (item->m_new){
			offset = strlen(item->m_new);
			memcpy(buf, item->m_new, offset);
		} else {
			offset = item->m_old_end > item->m_old_start ? item->m_old_end - item->m_old_start : 0;
			memcpy(buf, item->m_old_start, offset);	
		}
		buf += offset;
	}
	
	buf_start[len] = '\0';
	return buf_start;
}

#if 1
#include <assert.h>

int main(void)
{	
	pdu_paser_mgr_t *head = NULL;
	char  *str_1 = NULL, *str_2 = NULL;
	const char *str_3 = NULL;
	const char *str = 	"RTSP/1.0 200 OK\r\n"
						"Server: VLC/2.2.4\r\nContent-Length: 0\r\n"
						"Cseq: 2\r\n"
						"Public: DESCRIBE,SETUP,TEARDOWN,PLAY,PAUSE,GET_PARAMETER\r\n";

	assert((str_1 = (char*)g_alloc(100)));
	assert((str_2 = (char*)g_alloc(100)));
	
	strcpy(str_1, "str-1: xxx\r\n");
	strcpy(str_2, "str-2: xxxxxxx\r\n");
	
	assert((head = pdu_paser_mgr_paser(str, str + strlen(str), "\r\n")));
	head->m_new = str_1;
	if (head->m_next) head->m_next->m_new = str_2;
	pdu_paser_mgr_dump(head);
	
	assert((str_3 = pdu_paser_mgr_join(head)));
	MY_PRINTF("\nafter join:\n%s\n", str_3);
	pdu_paser_mgr_free(&head);
	g_free((void*)str_3);
	return 0;
}

#endif
