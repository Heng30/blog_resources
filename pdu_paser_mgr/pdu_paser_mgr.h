#ifndef _PDU_PASER_MGR_H_
#define _PDU_PASER_MGR_H_

/* item1: xxxxx\r\n
 * item2: xxxxx\r\n
 * item3: xxxxx\r\n
 * ...
 */
typedef struct _pdu_paser_mgr {
	const char *m_old_start; /* 旧的buf，指向item1开头 */
	const char *m_old_end; /* 旧的buf, 指向item1字段的结尾，此处为\r\n的\n后一个位置 */
	const char *m_new; /* 若需要更改old里的内容，此处为修改后的内容 */
	struct _pdu_paser_mgr *m_next;
} pdu_paser_mgr_t;

/* @func:
 *	解析pdu数据包，并产生一个管理节点链表
 * 
 * @param:
 *	pdu_start: 开头数据的位置
 *	pud_end: 结尾数据的后一个位置
 *	end_pos: 每一个数据的分割符
 */
pdu_paser_mgr_t* pdu_paser_mgr_paser(const char *pdu_start, const char *pdu_end, const char *end_pos);

/* @func:
 *	释放占用的内存
 */
void pdu_paser_mgr_free(pdu_paser_mgr_t **hdr);

/* @func:
 *	打印信息
 */
void pdu_paser_mgr_dump(pdu_paser_mgr_t *head);

/* @func:
 *	重组pdu数据包
 */
const char* pdu_paser_mgr_join(pdu_paser_mgr_t *head);


#endif
