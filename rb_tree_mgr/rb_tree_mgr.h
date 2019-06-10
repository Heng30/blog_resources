#ifndef _RB_TREE_MGR_H_
#define _RB_TREE_MGR_H_

#include <stdbool.h>

/* rb_tree_node_t的data的比较函数，1：左>右， 0：左==右， -1：左<右 */
typedef int (*rb_tree_node_cmp_t) (const void *ptr_1, const void *ptr_2);

/* rb_tree_node_t的data的释放函数 */
typedef void (*rb_tree_node_free_t) (void *ptr);

/* rb_tree_node_t的data的复制函数 */
typedef void* (*rb_tree_node_memcpy_t) (void *dst, const void *src, size_t size); 

typedef void* (*rb_tree_mgr_alloc_t) (size_t size);
typedef void (*rb_tree_mgr_free_t) (void *ptr);

typedef struct _rb_tree_node {
	void *m_data; // 真正要保存的数据
	unsigned char m_color;  // 红或黑色
	struct _rb_tree_node *m_parent;
	struct _rb_tree_node *m_left; 
	struct _rb_tree_node *m_right;
} rb_tree_node_t;

typedef struct _rb_tree_mgr {
	rb_tree_node_t *m_root;  // 树的根节点
	rb_tree_node_cmp_t m_cmp;  // rb_tree_node_t的data的比较函数，1：左>右， 0：左==右， -1：左<右
	rb_tree_node_free_t m_free; // rb_tree_node_t的data的释放函数
	rb_tree_node_memcpy_t m_cpy; // rb_tree_node_t的data的复制函数
	size_t m_max_node;	// 允许插入的最大节点数
	size_t m_cur_node;	// 当前的节点数
	pthread_mutex_t m_mutex;
} rb_tree_mgr_t;

/* @func:
 *	初始化管理器
 */
void rb_tree_mgr_init(rb_tree_mgr_alloc_t alloc, rb_tree_mgr_free_t dealloc);

/* @func:
 *	分配一个管理器
 */
rb_tree_mgr_t* rb_tree_mgr_new(rb_tree_node_free_t dealloc, rb_tree_node_cmp_t cmp, 
								rb_tree_node_memcpy_t cpy, size_t max_node);
								
/* @func:
 *	销毁管理器和树上的节点
 */
void rb_tree_mgr_free(rb_tree_mgr_t *tm);

/* @func:
 *	查找一个data节点
 */
rb_tree_node_t* rb_tree_mgr_node_find(rb_tree_mgr_t *tm, void *data);

/* @func:
 *	根据sample去查找，找到则保存副本到dst中
 */
void* rb_tree_node_cpy(rb_tree_mgr_t *tm, void *sample, void *dst, size_t size);


/* @func:
 *	插入节点
 */
bool rb_tree_mgr_insert(rb_tree_mgr_t *tm, void *data);

/* @func:
 *	删除一个节点
 */
bool rb_tree_mgr_del(rb_tree_mgr_t *tm, void *data);

/* @func:
 *	打印信息
 */
void rb_tree_mgr_dump(rb_tree_mgr_t *tm, bool is_dump_node);

#endif
