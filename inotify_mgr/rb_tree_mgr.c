#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "rb_tree_mgr.h"

#define RB_TREE_NODE_COLOR_RED 0X0
#define RB_TREE_NODE_COLOR_BLACK 0X1

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define RB_TREE_MGR_TRACE_LOG MY_PRINTF
#define RB_TREE_MGR_DEBUG_LOG MY_PRINTF
#define RB_TREE_MGR_INFO_LOG MY_PRINTF
#define RB_TREE_MGR_WARN_LOG MY_PRINTF
#define RB_TREE_MGR_ERROR_LOG MY_PRINTF

#define _TEST_
#undef _TEST_

#define _INOTIFY_TEST_

static rb_tree_mgr_alloc_t g_tm_alloc = NULL;
static rb_tree_mgr_free_t g_tm_free = NULL;

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

static bool _rb_tree_node_is_red (rb_tree_node_t *root)
{
	if (!root) return false;
  	return root->m_color == RB_TREE_NODE_COLOR_RED ? true : false;
}

static rb_tree_node_t *_rb_tree_node_new(void *data, unsigned char color)
{
	if (!data) return NULL;
 	rb_tree_node_t *tn = NULL;

	if (!(tn = (rb_tree_node_t*)g_tm_alloc(sizeof(rb_tree_node_t)))) {
		RB_TREE_MGR_WARN_LOG("g_tm_alloc error, errno: %d - %s", errno, strerror(errno));
		return NULL;
	}

	tn->m_color = color;
	tn->m_data = data;
  	return tn;
}

static void _rb_tree_node_left_rotate(rb_tree_mgr_t *tm, rb_tree_node_t *node)
{
	if (!tm || !node) return ;
	rb_tree_node_t *right = node->m_right;

	node->m_right = right->m_left;
	if (right->m_left) right->m_left->m_parent = node;
	right->m_parent = node->m_parent;

	if (!node->m_parent) tm->m_root = right;
	else {
		if (node->m_parent->m_left == node) node->m_parent->m_left = right;
		else node->m_parent->m_right = right;
	}

	right->m_left = node;
	node->m_parent = right;
}

static void _rb_tree_node_right_rotate(rb_tree_mgr_t *tm, rb_tree_node_t *node)
{
	if (!tm || !node) return ;
	rb_tree_node_t *left = node->m_left;

	node->m_left = left->m_right;
	if (left->m_right) left->m_right->m_parent = node;
	left->m_parent = node->m_parent;

	if (!node->m_parent) tm->m_root = left;
	else {
		if (node->m_parent->m_right == node) node->m_parent->m_right = left;
		else node->m_parent->m_left = left;
	}

	left->m_right = node;
	node->m_parent = left;
}

/* @func:
 *	初始化管理器
 */
void rb_tree_mgr_init(rb_tree_mgr_alloc_t alloc, rb_tree_mgr_free_t dealloc)
{
	if (!alloc || !dealloc) g_tm_alloc = _malloc2calloc, g_tm_free = free;
	else g_tm_alloc = alloc, g_tm_free = dealloc;
}

/* @func:
 *	分配一个管理器
 */
rb_tree_mgr_t* rb_tree_mgr_new(rb_tree_node_free_t dealloc, rb_tree_node_cmp_t cmp,
								rb_tree_node_memcpy_t cpy, size_t max_node)
{
	if (!cmp || !max_node) return NULL;
	if (!dealloc) dealloc = free;
	if (!cpy) cpy = memcpy;
	rb_tree_mgr_t *tm = NULL;

	if (!(tm = g_tm_alloc(sizeof(rb_tree_mgr_t)))) {
		RB_TREE_MGR_ERROR_LOG("g_tm_alloc error, errno: %d - %s", errno, strerror(errno));
		return NULL;
	}

	if (pthread_mutex_init(&tm->m_mutex, NULL)) {
		RB_TREE_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

	tm->m_free = dealloc, tm->m_cmp = cmp;
	tm->m_cpy = cpy, tm->m_max_node = max_node;
	return tm;

err:
	if (tm) g_tm_free(tm);
	return NULL;
}

/* @func:
 *	销毁管理器和树上的节点
 */
void rb_tree_mgr_free(rb_tree_mgr_t *tm)
{
	if (!tm) return ;
	rb_tree_node_t *it = NULL, *save = NULL;

	pthread_mutex_lock(&tm->m_mutex);
	for (it = tm->m_root; it; it = save) {
		if (!it->m_left) {
			save = it->m_right;
			tm->m_free(it->m_data);
			g_tm_free(it);
		} else {
			save = it->m_left;
			it->m_left = save->m_right;
			save->m_right= it;
		}
	}
	pthread_mutex_destroy(&tm->m_mutex);
	g_tm_free(tm);

	return ;
}

/* @func:
 *	查找一个rb_tree_node_t节点
 */
static rb_tree_node_t* _rb_tree_mgr_node_find(rb_tree_mgr_t *tm, void *data)
{
	if (!tm || !data) return NULL;
	rb_tree_node_t *it = NULL;
	int cmp = 0;

	for (it = tm->m_root; it; ) {
		cmp = tm->m_cmp(it->m_data, data);
		if (0 == cmp) return it;
		else if (cmp < 0) it = it->m_right;
		else it = it->m_left;
	}

	return NULL;
}

/* @func:
 *	查找一个data节点
 */
rb_tree_node_t* rb_tree_mgr_node_find(rb_tree_mgr_t *tm, void *data)
{
	if (!tm || !data) return NULL;
	rb_tree_node_t *tn = NULL;

	pthread_mutex_lock(&tm->m_mutex);
	tn = _rb_tree_mgr_node_find(tm, data);
	pthread_mutex_unlock(&tm->m_mutex);

	return tn;
}

/* @func:
 *	根据sample去查找，找到则保存副本到dst中
 */
void* rb_tree_node_cpy(rb_tree_mgr_t *tm, void *sample, void *dst, size_t size)
{
	if (!tm || !sample || !dst || !size) return NULL;
	rb_tree_node_t *tn = NULL;
	void *ret = NULL;

	pthread_mutex_lock(&tm->m_mutex);
	if ((tn = _rb_tree_mgr_node_find(tm, sample))) {
		tm->m_cpy(dst, tn->m_data, size);
		ret = dst;
	}
	pthread_mutex_unlock(&tm->m_mutex);

	return ret;
}

static void _rb_tree_insert_fixup(rb_tree_mgr_t *tm, rb_tree_node_t *tn)
{
	if (!tm || !tn) return ;
	rb_tree_node_t *tn_tmp = NULL;
	rb_tree_node_t *tn_parent = NULL, *tn_gparent = NULL, *tn_uncle = NULL;

	while ((tn_parent = tn->m_parent) && _rb_tree_node_is_red(tn_parent)) {
		tn_gparent = tn_parent->m_parent;

		if (tn_parent == tn_gparent->m_left) {
			// 情况1：叔叔节点是红色
			tn_uncle = tn_gparent->m_right;
			if (tn_uncle && _rb_tree_node_is_red(tn_uncle)) {
				tn_uncle->m_color = RB_TREE_NODE_COLOR_BLACK;
				tn_parent->m_color = RB_TREE_NODE_COLOR_BLACK;
				tn_gparent->m_color = RB_TREE_NODE_COLOR_RED;
				tn = tn_gparent;
				continue;
			}

			// 情况2：叔叔节点是黑色，且当前节点是右孩子
			if (tn_parent->m_right == tn) {
				_rb_tree_node_left_rotate(tm, tn_parent);
				tn_tmp = tn_parent;
				tn_parent = tn;
				tn = tn_tmp;
			}

			// 情况3：叔叔是黑色，且当前节点是左孩子
			tn_parent->m_color = RB_TREE_NODE_COLOR_BLACK;
			tn_gparent->m_color = RB_TREE_NODE_COLOR_RED;
			_rb_tree_node_right_rotate(tm, tn_gparent);
		} else {
			// 情况1：叔叔节点是红色
			tn_uncle = tn_gparent->m_left;
			if (tn_uncle && _rb_tree_node_is_red(tn_uncle)) {
				tn_uncle->m_color = RB_TREE_NODE_COLOR_BLACK;
				tn_parent->m_color = RB_TREE_NODE_COLOR_BLACK;
				tn_gparent->m_color = RB_TREE_NODE_COLOR_RED;
				tn = tn_gparent;
				continue;
			}

			// 情况2：叔叔节点是黑色，且当前节点是左孩子
			if (tn_parent->m_left == tn) {
				_rb_tree_node_right_rotate(tm, tn_parent);
				tn_tmp = tn_parent;
				tn_parent = tn;
				tn = tn_tmp;
			}

			// 情况3：叔叔是黑色，且当前节点是右孩子
			tn_parent->m_color = RB_TREE_NODE_COLOR_BLACK;
			tn_gparent->m_color = RB_TREE_NODE_COLOR_RED;
			_rb_tree_node_left_rotate(tm, tn_gparent);
		}
	}

	tm->m_root->m_color = RB_TREE_NODE_COLOR_BLACK;
}

static bool _rb_tree_mgr_insert(rb_tree_mgr_t *tm, void *data)
{
	if (!tm || !data) return false;
	rb_tree_node_t *tn_cur = NULL, *root = tm->m_root;
	rb_tree_node_t *tn = NULL;

	if (!(tn = _rb_tree_node_new(data, RB_TREE_NODE_COLOR_RED))) return false;

	while (root) {
		tn_cur = root;
		if (tm->m_cmp(tn_cur->m_data, tn->m_data) > 0) root = tn_cur->m_left;
		else root = tn_cur->m_right;
	}

	tn->m_parent = tn_cur;

	if (tn_cur) {
		if (tm->m_cmp(tn_cur->m_data, tn->m_data) > 0) tn_cur->m_left = tn;
		else tn_cur->m_right = tn;
	} else tm->m_root = tn;

	_rb_tree_insert_fixup(tm, tn);
	return true;
}

/* @func:
 *	插入节点
 */
bool rb_tree_mgr_insert(rb_tree_mgr_t *tm, void *data)
{
	if (!tm || !data) return false;
	bool ret = false;

	pthread_mutex_lock(&tm->m_mutex);
	if (tm->m_cur_node < tm->m_max_node) {
		if ((ret = _rb_tree_mgr_insert(tm, data)))
			tm->m_cur_node++;
	}
	pthread_mutex_unlock(&tm->m_mutex);
	return ret;
}


static void _rb_tree_delete_fixup(rb_tree_mgr_t *tm, rb_tree_node_t *tn, rb_tree_node_t *tn_parent)
{
	if (!tm) return ;
	rb_tree_node_t *tn_other = NULL;

	while ((!tn || !_rb_tree_node_is_red(tn)) && tn != tm->m_root) {
		if (tn_parent->m_left == tn) {
			tn_other = tn_parent->m_right;
			if (_rb_tree_node_is_red(tn_other)) {
				// 兄弟节点是红色
				tn_other->m_color = RB_TREE_NODE_COLOR_BLACK;
				tn_parent->m_color = RB_TREE_NODE_COLOR_RED;
				_rb_tree_node_left_rotate(tm, tn_parent);
				tn_other = tn_parent->m_right;
			}

			if ((!tn_other->m_left || !_rb_tree_node_is_red(tn_other->m_left)) &&
				(!tn_other->m_right || !_rb_tree_node_is_red(tn_other->m_right))) {
					// 兄弟节点是黑色，且兄弟节点的两个孩子都是黑色
					tn_other->m_color = RB_TREE_NODE_COLOR_RED;
					tn = tn_parent;
					tn_parent = tn->m_parent;
			} else {
				if (!tn_other->m_right || !_rb_tree_node_is_red(tn_other->m_right)) {
					// 兄弟节点是黑色，且兄弟节点的左孩子是红色，右孩子是黑色
					tn_other->m_left->m_color = RB_TREE_NODE_COLOR_BLACK;
					tn_other->m_color = RB_TREE_NODE_COLOR_RED;
					_rb_tree_node_right_rotate(tm, tn_other);
					tn_other = tn_parent->m_right;
				}

				// 兄弟节点是黑色，且兄弟节点的右孩子是红色，左孩子任意颜色
				tn_other->m_color = tn_parent->m_color;
				tn_parent->m_color = RB_TREE_NODE_COLOR_BLACK;
				tn_other->m_right->m_color = RB_TREE_NODE_COLOR_BLACK;
				_rb_tree_node_left_rotate(tm, tn_parent);
				tn = tm->m_root;
				break;
			}
		} else {
			tn_other = tn_parent->m_left;
			if (_rb_tree_node_is_red(tn_other)) {
				// 兄弟节点是红色
				tn_other->m_color = RB_TREE_NODE_COLOR_BLACK;
				tn_parent->m_color = RB_TREE_NODE_COLOR_RED;
				_rb_tree_node_right_rotate(tm, tn_parent);
				tn_other = tn_parent->m_left;
			}

			if ((!tn_other->m_left || !_rb_tree_node_is_red(tn_other->m_left)) &&
				(!tn_other->m_right || !_rb_tree_node_is_red(tn_other->m_right))) {
					// 兄弟节点是黑色，且兄弟节点的两个孩子都是黑色
					tn_other->m_color = RB_TREE_NODE_COLOR_RED;
					tn = tn_parent;
					tn_parent = tn->m_parent;
			} else {
				if (!tn_other->m_left || !_rb_tree_node_is_red(tn_other->m_left)) {
					// 兄弟节点是黑色，且兄弟节点的左孩子是红色，右孩子是黑色
					tn_other->m_right->m_color = RB_TREE_NODE_COLOR_BLACK;
					tn_other->m_color = RB_TREE_NODE_COLOR_RED;
					_rb_tree_node_left_rotate(tm, tn_other);
					tn_other = tn_parent->m_left;
				}

				// 兄弟节点是黑色，且兄弟节点的右孩子是红色，左孩子任意颜色
				tn_other->m_color = tn_parent->m_color;
				tn_parent->m_color = RB_TREE_NODE_COLOR_BLACK;
				tn_other->m_left->m_color = RB_TREE_NODE_COLOR_BLACK;
				_rb_tree_node_right_rotate(tm, tn_parent);
				tn = tm->m_root;
				break;
			}
		}
	}

	if (tn) tn->m_color = RB_TREE_NODE_COLOR_BLACK;
}

bool _rb_tree_mgr_del(rb_tree_mgr_t *tm, void *data)
{
 	if (!tm || !data) return false;
	rb_tree_node_t *tn= NULL;
	rb_tree_node_t *tn_child = NULL, *tn_parent = NULL, *tn_replace = NULL;
	unsigned char color = 0;

	if (!(tn = _rb_tree_mgr_node_find(tm, data))) return false;

	if (tn->m_left && tn->m_right) {
		tn_replace = tn->m_right;
		// 获取右子树的最小节点, 用于替换被删除节点
		while (tn_replace->m_left) tn_replace = tn_replace->m_left;

		// 删除节点
		if (tn->m_parent) {
			if (tn->m_parent->m_left == tn) tn->m_parent->m_left = tn_replace;
			else tn->m_parent->m_right = tn_replace;
		} else tm->m_root = tn_replace;

		// 此时tn_replace一定不存在左孩子
		tn_child = tn_replace->m_right;
		tn_parent = tn_replace->m_parent;
		color = tn_replace->m_color;

		// 修正节点的父子关系
		if (tn_parent == tn) tn_parent = tn_replace;
		else {
			if (tn_child) tn_child->m_parent = tn_parent;
			tn_parent->m_left = tn_child;
			tn_replace->m_right = tn->m_right;
			tn->m_right->m_parent = tn_replace;
		}

		tn_replace->m_parent = tn->m_parent;
		tn_replace->m_color = tn->m_color;
		tn_replace->m_left = tn->m_left;
		tn->m_left->m_parent = tn_replace;
	} else {
		if (tn->m_left) tn_child = tn->m_left;
		else tn_child = tn->m_right;

		tn_parent = tn->m_parent;
		color = tn->m_color;

		if (tn_child) tn_child->m_parent = tn_parent;

		if (tn_parent) {
			if (tn_parent->m_left == tn) tn_parent->m_left = tn_child;
			else tn_parent->m_right = tn_child;
		} else tm->m_root = tn_child;
	}

	if (color == RB_TREE_NODE_COLOR_BLACK) _rb_tree_delete_fixup(tm, tn_child, tn_parent);
	tm->m_free(tn->m_data); g_tm_free(tn);

	return true;
}

/* @func:
 *	删除一个节点
 */
bool rb_tree_mgr_del(rb_tree_mgr_t *tm, void *data)
{
	if (!tm || !data) return false;
	bool ret = false;

	pthread_mutex_lock(&tm->m_mutex);
	if ((ret = _rb_tree_mgr_del(tm, data))) tm->m_cur_node--;
	pthread_mutex_unlock(&tm->m_mutex);

	return ret;
}

#ifdef _TEST_
typedef struct _node _node_t;
struct _node {
	int m_num;
	size_t m_black_count;
};
#endif

static void _rb_tree_node_dump(rb_tree_node_t *root)
{
	if (!root) return ;

	RB_TREE_MGR_TRACE_LOG("++++++");
#ifdef _TEST_
	RB_TREE_MGR_TRACE_LOG("data_value: %d", ((_node_t*)root->m_data)->m_num);
	RB_TREE_MGR_TRACE_LOG("data_black_count: %lu", ((_node_t*)root->m_data)->m_black_count),
#endif

#ifdef _INOTIFY_TEST_	
	#include "inotify_mgr.h"
	
	RB_TREE_MGR_TRACE_LOG("wfd: %d", ((wfd_node_t*)root->m_data)->m_wfd);
	RB_TREE_MGR_TRACE_LOG("parent_wfd: %d", ((wfd_node_t*)root->m_data)->m_parent_wfd);
	RB_TREE_MGR_TRACE_LOG("name: %s", ((wfd_node_t*)root->m_data)->m_name);
	RB_TREE_MGR_TRACE_LOG("child_wfd_tree: %p", ((wfd_node_t*)root->m_data)->m_child_wfd_tree);
#else
	RB_TREE_MGR_TRACE_LOG("data: %p", root->m_data);
	RB_TREE_MGR_TRACE_LOG("color: %s", root->m_color == RB_TREE_NODE_COLOR_RED ? "red" : "black");
	RB_TREE_MGR_TRACE_LOG("parent: %p", root->m_parent);
	RB_TREE_MGR_TRACE_LOG("self: %p", root);
	RB_TREE_MGR_TRACE_LOG("left: %p", root->m_left);
	RB_TREE_MGR_TRACE_LOG("right: %p", root->m_right);
#endif

	RB_TREE_MGR_TRACE_LOG("++++++\n");
	_rb_tree_node_dump(root->m_left);
	_rb_tree_node_dump(root->m_right);
}

/* @func:
 *	打印信息
 */
void rb_tree_mgr_dump(rb_tree_mgr_t *tm, bool is_dump_node)
{
	if (!tm) return ;

	pthread_mutex_lock(&tm->m_mutex);
	RB_TREE_MGR_TRACE_LOG("==========");
	RB_TREE_MGR_TRACE_LOG("root: %p", tm->m_root);
	RB_TREE_MGR_TRACE_LOG("cmp: %p", tm->m_cmp);
	RB_TREE_MGR_TRACE_LOG("cpy: %p", tm->m_cpy);
	RB_TREE_MGR_TRACE_LOG("free: %p", tm->m_free);
	RB_TREE_MGR_TRACE_LOG("max_node: %lu", tm->m_max_node);
	RB_TREE_MGR_TRACE_LOG("cur_node: %lu", tm->m_cur_node);
	if (is_dump_node) _rb_tree_node_dump(tm->m_root);
	RB_TREE_MGR_TRACE_LOG("==========");
	pthread_mutex_unlock(&tm->m_mutex);
}


/*====================test=====================*/
#ifdef _TEST_
/* 红黑树的特征：
 *	1. 每个节点要么是红色，要么是黑色 （不需要测试）
 *	2. 每个叶子节点（尾端的nil指针或NULL节点）是黑色 （无需测试）
 *	3. 不能有两个红色节点是父子关系
 *	4. 对任意节点而言，从该节点出发到叶子节点经过的黑色节点个数相同
 *	5. 根节点是黑色
 */
#include <assert.h>

static int _cmp(const void *ptr_1, const void *ptr_2)
{
	if (!ptr_1 || !ptr_2) return 0;
	if (ptr_1 == ptr_2) return 0;

	if (((struct _node*)ptr_1)->m_num > ((struct _node*)ptr_2)->m_num) return 1;
	else if (((struct _node*)ptr_1)->m_num < ((struct _node*)ptr_2)->m_num) return -1;
	return 0;
}

/* 测试没有两个红色节点是父子关系 */
static void _test_no_red_node_is_parent_child_relationship(rb_tree_node_t *root)
{
	if (!root) return ;

	if (_rb_tree_node_is_red(root)) {
		if (root->m_parent) assert(!_rb_tree_node_is_red(root->m_parent));
		if (root->m_left) assert(!_rb_tree_node_is_red(root->m_left));
		if (root->m_right) assert(!_rb_tree_node_is_red(root->m_right));
	}

	_test_no_red_node_is_parent_child_relationship(root->m_left);
	_test_no_red_node_is_parent_child_relationship(root->m_right);
}

/* 测试特征4 */
static void _set_every_path_black_node_count(rb_tree_node_t *root)
{
	if (!root) return ;

	if (root->m_parent) {
		if (!_rb_tree_node_is_red(root)) ((struct _node*)root->m_data)->m_black_count = ((struct _node*)root->m_parent->m_data)->m_black_count + 1;
		else ((struct _node*)root->m_data)->m_black_count = ((struct _node*)root->m_parent->m_data)->m_black_count;
	} else ((struct _node*)root->m_data)->m_black_count = 1;

	_set_every_path_black_node_count(root->m_left);
	_set_every_path_black_node_count(root->m_right);
}

static void _test_every_path_black_node_count_is_same(rb_tree_node_t *root, size_t *black_count)
{
	if (!root || !black_count) return ;

	if (!root->m_left && !root->m_right) {
		if (*black_count == 0) *black_count = ((struct _node*)root->m_data)->m_black_count;
		else {
			// MY_PRINTF("black_count: %lu - %lu", *black_count, ((struct _node*)root->m_data)->m_black_count);
			assert(*black_count == ((struct _node*)root->m_data)->m_black_count);
		}
	}
	_test_every_path_black_node_count_is_same(root->m_left, black_count);
	_test_every_path_black_node_count_is_same(root->m_right, black_count);
}

int main()
{
	pthread_t pt[10];
	size_t i = 0;
	rb_tree_mgr_t *tm = NULL;

	rb_tree_mgr_init(NULL, NULL);
	assert((tm = rb_tree_mgr_new(NULL, _cmp, NULL, 102400)));
	rb_tree_mgr_dump(tm, true);

	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
		pthread_create(&pt[i], NULL, ({
			void* _(void *arg) {
				size_t j = 0, k = 0;
				struct _node *node = NULL;
				struct _node node_value = {0, 0};
				struct _node sample = {0, 0}, dst = {0, 0};
				size_t max_node = 256;
				size_t black_count = 0;

				for (k = 0; k < max_node; k++) {
					for (j = 0; j < k; j++) {
						if (!(node = malloc(sizeof(struct _node)))) continue;
						black_count = 0;
						node->m_num = node_value.m_num = j;
						sample.m_num = j;

						assert((rb_tree_mgr_insert(tm, node)));
						assert(rb_tree_mgr_node_find(tm, &node_value));
						assert(sample.m_num == ((struct _node*)rb_tree_node_cpy(tm, &sample, &dst, sizeof(struct _node)))->m_num);

						pthread_mutex_lock(&tm->m_mutex);
						if (tm->m_root) {
							/* 测试跟节点是黑色(特征5) */
							assert(!_rb_tree_node_is_red(tm->m_root));
							/* 测试没有两个红色节点是父子关系(特征3) */
							_test_no_red_node_is_parent_child_relationship(tm->m_root);

							/* 测试特征4 */
							_set_every_path_black_node_count(tm->m_root);
							_test_every_path_black_node_count_is_same(tm->m_root, &black_count);
						}
						pthread_mutex_unlock(&tm->m_mutex);
					}

					for (j = 0; j < k; j++) {
						black_count = 0;
						node_value.m_num = j;
						assert(rb_tree_mgr_del(tm, &node_value));

						pthread_mutex_lock(&tm->m_mutex);
						if (tm->m_root) {
							/* 测试跟节点是黑色(特征5) */
							assert(!_rb_tree_node_is_red(tm->m_root));
							/* 测试没有两个红色节点是父子关系(特征3) */
							_test_no_red_node_is_parent_child_relationship(tm->m_root);

							/* 测试特征4 */
							_set_every_path_black_node_count(tm->m_root);
							_test_every_path_black_node_count_is_same(tm->m_root, &black_count);
						}
						pthread_mutex_unlock(&tm->m_mutex);
					}
				}
				return arg;
			}; _;}), NULL);
	}

	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++)
		pthread_join(pt[i], NULL);

	rb_tree_mgr_dump(tm, true);
	rb_tree_mgr_free(tm);
	MY_PRINTF("OK");
	return 0;
}
#endif
