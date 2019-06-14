#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>

#include "inotify_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define INOTIFY_MGR_WARN_LOG MY_PRINTF
#define INOTIFY_MGR_DEBUG_LOG MY_PRINTF
#define INOTIFY_MGR_ERROR_LOG MY_PRINTF
#define INOTIFY_MGR_INFO_LOG MY_PRINTF
#define INOTIFY_MGR_TRACE_LOG MY_PRINTF

/* 扫描目录，将待扫描的目录加到链表中 */
typedef struct _wfd_list_node {
	int m_parent_wfd;
	const char *m_dir; // 待扫描的子目录，绝对路径
	const char *m_name; // 子目录名字, 是引用，不需要free，会传递给wfd_node_t
	struct _wfd_list_node *m_next;
} wfd_list_node_t;

/* 保存子目录关联的描述符，用于删除子目录 */
typedef struct _wfd_remove_node {
	int m_wfd;
	struct _wfd_remove_node *m_next;
} wfd_remove_node_t;

static inotify_mgr_alloc_t g_im_alloc = NULL;
static inotify_mgr_free_t g_im_free = NULL;

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

/* ========= [START]: 扫描目录添加到待扫描目录链表 =================== */
static void _wfd_list_node_add(wfd_list_node_t **head, wfd_list_node_t *item)
{
	if (!head || !item) return ;
	item->m_next = *head;
	*head = item;
}

static wfd_list_node_t* _wfd_list_node_pop(wfd_list_node_t **head)
{
	if (!head || !(*head)) return NULL;
	wfd_list_node_t *wln = *head;
	*head = (*head)->m_next;
	return wln;
}

static void _wfd_list_node_free(wfd_list_node_t *wln)
{
	if (!wln) return ;
	g_im_free((void*)wln->m_dir);
	g_im_free(wln);
}

static void _wfd_list_node_dump(wfd_list_node_t *wln)
{
	if (!wln) return ;

	INOTIFY_MGR_TRACE_LOG("---wfd_list_node_start-----");
	INOTIFY_MGR_TRACE_LOG("parent_wfd: %d", wln->m_parent_wfd);
	INOTIFY_MGR_TRACE_LOG("dir: %s", wln->m_dir);
	INOTIFY_MGR_TRACE_LOG("name: %s", wln->m_name);
	INOTIFY_MGR_TRACE_LOG("next: %p", wln->m_next);
	INOTIFY_MGR_TRACE_LOG("---wfd_list_node_end-----");
}

static wfd_list_node_t* _wfd_list_node_new(int parent_wfd, const char *dir, const char *name)
{
	if (!dir || !name) return NULL;
	int dir_len = strlen(dir) + 1;
	int name_len = strlen(name) + 2;
	char *new_dir = NULL, *new_name = NULL;
	wfd_list_node_t *wln = NULL;


	if (!(wln = (wfd_list_node_t*)g_im_alloc(sizeof(wfd_list_node_t)))) {
		INOTIFY_MGR_ERROR_LOG("g_im_alloc error, errno: %d - %s", errno, strerror(errno));
        goto err;
	}

	if (!(new_dir = (char*)g_im_alloc(dir_len))) {
		INOTIFY_MGR_ERROR_LOG("g_im_alloc error, errno: %d - %s", errno, strerror(errno));
        goto err;
	}

	if (!(new_name = (char*)g_im_alloc(name_len))) {
	    INOTIFY_MGR_WARN_LOG("g_im_alloc error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

	snprintf(new_dir, dir_len, "%s", dir);
	snprintf(new_name, name_len, "%s/", name);

	wln->m_dir = new_dir;
	wln->m_name = new_name;
	wln->m_parent_wfd = parent_wfd;
	wln->m_next = NULL;
	return wln;

err:
	if (new_dir) g_im_free(new_dir);
	if (new_name) g_im_free(new_name);
	if (wln) g_im_free(wln);
	return NULL;
}

/* @func:
 *	扫描一级目录
 */
static void _wfd_list_node_scan_dir(wfd_list_node_t **wln_head, const char *dir_root, int parent_wfd)
{
	if (!dir_root || !wln_head) return ;
	wfd_list_node_t *wln = NULL;
	char dir_buf[PATH_MAX] = {0};
	DIR *pdir = NULL;
	struct dirent *pdirent = NULL;
	struct stat st;

	if (!(pdir = opendir(dir_root))) {
		INOTIFY_MGR_WARN_LOG("opendir %s error, errno: %d - %s", dir_root, errno, strerror(errno));
		return ;
	}

	for (pdirent = readdir(pdir); pdirent; pdirent = readdir(pdir)) {
		if (!strcmp(pdirent->d_name, ".") || !strcmp(pdirent->d_name, "..")) continue;

		snprintf(dir_buf, sizeof(dir_buf), "%s/%s", dir_root, pdirent->d_name);
		if (lstat(dir_buf, &st)) {
			INOTIFY_MGR_WARN_LOG("lstat %s error, errno: %d - %s", pdirent->d_name, errno, strerror(errno));
			continue;
		}

		if (!S_ISDIR(st.st_mode)) continue;

		if (!(wln = _wfd_list_node_new(parent_wfd, dir_buf, pdirent->d_name))) continue;
		_wfd_list_node_add(wln_head, wln);
	}

	closedir(pdir);
}

/* ========= [END]: 扫描目录添加到待扫描目录链表 =================== */




/* ========= [START]: 目录移出监听目录时，获取子目录的描述符，由于销毁挂在树上的节点 =================== */

static void _wfd_remove_node_list_free(wfd_remove_node_t *wrn)
{
	if (!wrn) return ;
	g_im_free(wrn);
}

static void _wfd_remove_node_list_add(wfd_remove_node_t **head, wfd_remove_node_t *item)
{
	if (!head || !item) return ;
	item->m_next = *head;
	*head = item;
}

static wfd_remove_node_t* _wfd_remove_node_list_pop(wfd_remove_node_t **head)
{
	if (!head || !(*head)) return NULL;
	wfd_remove_node_t *wrn = *head;
	*head = (*head)->m_next;
	return wrn;
}

static void _wfd_remove_node_list_dump(wfd_remove_node_t *wrn)
{
	if (!wrn) return ;

	INOTIFY_MGR_TRACE_LOG("---wfd_remove_node_list_start-----");
	INOTIFY_MGR_TRACE_LOG("wfd: %d", wrn->m_wfd);
	INOTIFY_MGR_TRACE_LOG("next: %p", wrn->m_next);
	INOTIFY_MGR_TRACE_LOG("---wfd_remove_node_list_end-----");
}

static void _scan_wfd_child_tree_and_add_to_remove_list(wfd_remove_node_t **head, rb_tree_node_t *tn)
{
	if (!head || !tn) return ;
	int wfd = 0;
	wfd_remove_node_t *wrn = NULL;

	if ((wfd = *(int*)tn->m_data) < 0) return ;
	if (!(wrn = (wfd_remove_node_t*)g_im_alloc(sizeof(wfd_remove_node_t)))) {
		INOTIFY_MGR_WARN_LOG("g_im_alloc error, errno: %d - %s", errno, strerror(errno));
		return ;
	}

	wrn->m_wfd = wfd;
	_wfd_remove_node_list_add(head, wrn);
	_scan_wfd_child_tree_and_add_to_remove_list(head, tn->m_left);
	_scan_wfd_child_tree_and_add_to_remove_list(head, tn->m_right);
	return ;
}

/* ========= [END]: 目录移出监听目录时，获取子目录的描述符，由于销毁挂在树上的节点 =================== */


/* =========[START]: 监听目录的子树上的节点 ================= */
static void _child_wfd_node_free(void *ptr)
{
	g_im_free(ptr);
}

static int _child_wfd_node_cmp(const void *ptr_1, const void *ptr_2)
{
	if (ptr_1 == ptr_2) return 0;
	if (!ptr_1 || !ptr_2) return 1;

	if (*(int*)ptr_1 > *(int*)ptr_2) return 1;
	else if (*(int*)ptr_1 < *(int*)ptr_2) return -1;
	return 0;
}

static void* _child_wfd_node_memcpy(void *dst, const void *src, size_t size)
{
	if (!dst || !src || !size) return NULL;
	memcpy(dst, src, size);
	return dst;
}

/* =========[END]: 监听目录的子树上的节点 ================= */


/* =========[START]: 监听目录树上的节点 ================= */

wfd_node_t* _wfd_node_new(int parent_wfd, int wfd, const char *name)
{
    if (!name || wfd < 0) return NULL;
    wfd_node_t *wn = NULL;

    if (!(wn = (wfd_node_t*)g_im_alloc(sizeof(wfd_node_t)))) {
        INOTIFY_MGR_ERROR_LOG("g_im_alloc error, errno: %d - %s", errno, strerror(errno));
        goto err;
    }

	wn->m_child_wfd_tree = rb_tree_mgr_new(_child_wfd_node_free, _child_wfd_node_cmp,
											_child_wfd_node_memcpy, INT_MAX);

	if (!wn->m_child_wfd_tree) goto err;

    wn->m_wfd = wfd;
    wn->m_parent_wfd = parent_wfd;
    wn->m_name = name;
    return wn;

err:
	if (wn) {
		if (wn->m_child_wfd_tree) rb_tree_mgr_free(wn->m_child_wfd_tree);
		g_im_free(wn);
	}
	return NULL;
}

static int _wfd_node_cmp(const void *ptr_1, const void *ptr_2)
{
	if (ptr_1 == ptr_2) return 0;
	if (!ptr_1 || !ptr_2) return 1;

	if (((wfd_node_t*)ptr_1)->m_wfd > ((wfd_node_t*)ptr_2)->m_wfd) return 1;
	else if (((wfd_node_t*)ptr_1)->m_wfd < ((wfd_node_t*)ptr_2)->m_wfd) return -1;
	return 0;
}

static void* _wfd_node_memcpy(void *dst, const void *src, size_t size)
{
	if (!dst || !src || !size) return NULL;
	memcpy(dst, src, size);
	return dst;
}

static void _wfd_node_free(void *ptr)
{
    if (!ptr) return ;
	wfd_node_t *wn = (wfd_node_t*)ptr;

    if (wn->m_name) g_im_free((char*)wn->m_name);
	if (wn->m_child_wfd_tree) rb_tree_mgr_free(wn->m_child_wfd_tree);
    g_im_free(wn);
}

/* =========[END]: 监听目录树上的节点 ================= */


/* @func:
 *	添加一个监听的目录
 */
static void _inotify_mgr_add_watch(inotify_mgr_t *im, struct inotify_event *ie, bool is_rename)
{
	if (!im || !ie) return ;
    if (ie->len <= 0) return ;
	int wfd = 0;
    wfd_node_t *wn = NULL, *parent_wn = NULL;
    char path[PATH_MAX] = {0};
	wfd_list_node_t *wln = NULL, *wln_old = NULL;
	int *child_wfd = NULL;
	wfd_node_t wn_sample;
	rb_tree_node_t *tn_ret = NULL;
	int path_len = 0;

	if (!inotify_mgr_path_get(path, sizeof(path), im, ie)) return ;
	if (is_rename) {
		path_len = strlen(path);
		path[path_len <= 0 ? 0 : path_len - 1] = '\0'; /* 去除结尾的 '\' */
	}
	if (!(wln = _wfd_list_node_new(ie->wd, path, ie->name))) return ;

	while (wln) {
		//_wfd_list_node_dump(wln);
		if ((wfd = inotify_add_watch(im->m_ifd, wln->m_dir, IN_ALL_EVENTS)) < 0) {
			INOTIFY_MGR_WARN_LOG("inotify_add_watch %s error, errno: %d - %s", wln->m_dir, errno, strerror(errno));
			wln_old = _wfd_list_node_pop(&wln);
			g_im_free((void*)wln_old->m_name);
			g_im_free((void*)wln_old->m_dir);
			g_im_free((void*)wln_old);
			continue;
		}

		/* 已经在监听了，不能重复添加 */
		wn_sample.m_wfd = wfd;
		if ((tn_ret = rb_tree_mgr_node_find(im->m_wfd_tree, (void*)&wn_sample))) {
			wln_old = _wfd_list_node_pop(&wln);

			/*	重命名时需要重新扫描被重命名的目录;
			 *	由于创建后立刻重命名时，对于inotify event异步处理事件慢过重命名，
			 *  导致部分目录由于重命名无法监听，重新扫描一次重命名目录，将没有添加监听的添加监听
			 */
			if (is_rename) _wfd_list_node_scan_dir(&wln, wln_old->m_dir, wfd);

			g_im_free((void*)wln_old->m_name);
			g_im_free((void*)wln_old->m_dir);
			g_im_free((void*)wln_old);
			continue;
		}

		wn = _wfd_node_new(wln->m_parent_wfd, wfd, wln->m_name);
		rb_tree_mgr_insert(im->m_wfd_tree, (void*)wn);

		/* 设置根目录的监听描述符 */
		if (im->m_wfd < 0 && wln->m_parent_wfd < 0) im->m_wfd = wfd;

		/* 添加到父节点的子目录树中 */
		wn_sample.m_wfd = wln->m_parent_wfd;
		if ((tn_ret = rb_tree_mgr_node_find(im->m_wfd_tree, (void*)&wn_sample))) {
			if ((parent_wn = (wfd_node_t*)tn_ret->m_data)) {
				if ((child_wfd = g_im_alloc(sizeof(int)))) {
					*child_wfd = wfd;
					rb_tree_mgr_insert(parent_wn->m_child_wfd_tree, (void*)child_wfd);
				}
			}
		}

		wln_old = _wfd_list_node_pop(&wln);
		_wfd_list_node_scan_dir(&wln, wln_old->m_dir, wfd);
		_wfd_list_node_free(wln_old);
	}
}

/* @func:
 *	移除一个监听的文件或目录
 */
static bool _inotify_mgr_rm_watch(inotify_mgr_t *im, struct inotify_event *ie)
{
	if (!im || !ie) return false;
  	wfd_node_t old_wn = {
		.m_wfd = ie->wd,
		.m_parent_wfd = -1,
		.m_name = NULL,
	};

	inotify_rm_watch(im->m_ifd, ie->wd);
	rb_tree_mgr_del(im->m_wfd_tree, (void*)&old_wn);

	return true;
}

/* @func:
 *	重命名一个文件夹
 */
static bool _inotify_mgr_rename_watch(inotify_mgr_t *im, int wfd, int parent_wfd, const char *new_name)
{
    if (!im || wfd < 0 || !new_name) return false;
	rb_tree_node_t *tn = NULL;
	wfd_node_t *new_wn = NULL;
	wfd_node_t old_wn = {
		.m_wfd = wfd,
		.m_parent_wfd = -1,
		.m_name = NULL,
	};

	char buf[sizeof(struct inotify_event) + PATH_MAX] = {0};
	struct inotify_event *ie = (struct inotify_event*)buf;
	ie->wd = wfd; ie->len = 1;
    memcpy(buf + sizeof(struct inotify_event), "\0", ie->len);

	if ((tn = rb_tree_mgr_node_find(im->m_wfd_tree, (void*)&old_wn))) {
		if (!(new_wn = (wfd_node_t*)tn->m_data)) {
			rb_tree_mgr_del(im->m_wfd_tree, (void*)&old_wn);
            return false;
		}
		g_im_free((char*)new_wn->m_name);
		new_wn->m_name = new_name;
		new_wn->m_parent_wfd = parent_wfd;
		_inotify_mgr_add_watch(im, ie, true);
	}
    return true;
}


/* @func:
 *	移除自身及子目录下所有的监听描述符，并清理占用的内存
 */
static void _child_wfd_node_remove_watch(inotify_mgr_t *im, wfd_node_t *wn)
{
	if (!im || !wn) return ;
	int child_wfd = 0, parent_wfd = 0;
	rb_tree_node_t *tn = NULL, *tn_child = NULL;
	wfd_remove_node_t *wrn = NULL, *wrn_old = NULL;
	wfd_node_t wn_sample;

	if (!(wrn = (wfd_remove_node_t*)g_im_alloc(sizeof(wfd_remove_node_t)))) {
		INOTIFY_MGR_WARN_LOG("g_im_alloc error, errno: %d - %s", errno, strerror(errno));
		return ;
	}

	wrn->m_wfd = wn->m_wfd, wrn->m_next = NULL;
	parent_wfd = wn->m_parent_wfd;
	child_wfd = wn->m_wfd;

	while (wrn) {
		wn_sample.m_wfd = wrn->m_wfd;
		if (!(tn = rb_tree_mgr_node_find(im->m_wfd_tree, (void*)&wn_sample))) {
			wrn_old = _wfd_remove_node_list_pop(&wrn);
			goto clean;
		}

		if (!(wn = (wfd_node_t*)tn->m_data)) {
			wrn_old = _wfd_remove_node_list_pop(&wrn);
			goto clean;
		}

		if (!wn->m_child_wfd_tree) {
			wrn_old = _wfd_remove_node_list_pop(&wrn);
			goto clean;
		}

		tn_child = wn->m_child_wfd_tree->m_root;
		wrn_old = _wfd_remove_node_list_pop(&wrn);
		_scan_wfd_child_tree_and_add_to_remove_list(&wrn, tn_child);

clean:
		inotify_rm_watch(im->m_ifd, wn_sample.m_wfd);
		_wfd_remove_node_list_free(wrn_old);
		rb_tree_mgr_del(im->m_wfd_tree, &wn_sample);
	}

	/* 清除父节点中保存的子节点的描述符 */
	wn_sample.m_wfd = parent_wfd;
	if (!(tn = rb_tree_mgr_node_find(im->m_wfd_tree, (void*)&wn_sample))) return ;
	if (!(wn = (wfd_node_t*)tn->m_data)) return ;
	if (!wn->m_child_wfd_tree) return ;
	rb_tree_mgr_del(wn->m_child_wfd_tree, &child_wfd);
}

/* @func:
 *	移出监控目录范围
 */
static bool _inotify_mgr_move_out_watch(inotify_mgr_t *im, struct inotify_event *ie)
{
	if (!im || !ie) return false;
	rb_tree_node_t * tn = NULL;
	wfd_node_t *wn = NULL;
	wfd_node_t old_wn = {
		.m_wfd = ie->wd,
		.m_parent_wfd = -1,
		.m_name = NULL,
	};

	if (!(tn = rb_tree_mgr_node_find(im->m_wfd_tree, &old_wn))) goto err;
	if (!tn->m_data) goto err;

	wn = (wfd_node_t*)tn->m_data;
	_child_wfd_node_remove_watch(im, wn);
	return true;

err:
	return false;
}




/* @func:
 *	初始化管理器
 */
void inotify_mgr_init(inotify_mgr_alloc_t alloc, inotify_mgr_free_t dealloc)
{
	if (!alloc || !dealloc) g_im_alloc = _malloc2calloc, g_im_free = free;
	else g_im_alloc = alloc, g_im_free = dealloc;
}

/* @func:
 *	创建一个管理器
 */
inotify_mgr_t* inotify_mgr_new(const char *path, inotify_mgr_func_t func, void *arg)
{
	if (!func || !path) return NULL;
	inotify_mgr_t *im = NULL;
	char buf[sizeof(struct inotify_event) + PATH_MAX] = {0};
	struct inotify_event *ie = (struct inotify_event*)buf;

	ie->wd = -1; ie->len = strlen(path);
    memcpy(buf + sizeof(struct inotify_event), path, ie->len);

	if (!(im = g_im_alloc(sizeof(inotify_mgr_t)))) {
		INOTIFY_MGR_ERROR_LOG("g_im_alloc error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

	if ((im->m_ifd = inotify_init()) < 0) {
		INOTIFY_MGR_ERROR_LOG("inotify_init error, errno: %d - %s", errno, strerror(errno));
		goto err;
	}

	if (!(im->m_wfd_tree = rb_tree_mgr_new(_wfd_node_free, _wfd_node_cmp,
											_wfd_node_memcpy, INT_MAX))) goto err;

	im->m_func = func, im->m_arg = arg, im->m_wfd = -1;
    _inotify_mgr_add_watch(im, ie, false);
	return im;

err:
	if (im) {
		if (im->m_ifd > 0) close(im->m_ifd);
		if (im->m_wfd_tree) rb_tree_mgr_free(im->m_wfd_tree);
		g_im_free(im);
	}
	return NULL;
}

/* @func:
 *	销毁一个管理器
 */
void inotify_mgr_free(inotify_mgr_t *im)
{
	if (!im) return ;
	wfd_node_t *wn = NULL;
	rb_tree_node_t *tn = NULL;
	wfd_node_t wn_sample;
	wn_sample.m_wfd = im->m_wfd;

	if (im->m_wfd_tree) goto clean;
	if (!(tn = rb_tree_mgr_node_find(im->m_wfd_tree, (void*)&wn_sample))) goto clean;
	if (!(wn = (wfd_node_t*)tn->m_data)) goto clean;

	_child_wfd_node_remove_watch(im, wn);

clean:
	rb_tree_mgr_free(im->m_wfd_tree);
	close(im->m_ifd);
	g_im_free(im);
}

/* @func:
 *	监听的根目录是否删除了
 */
bool inotify_mgr_is_empty(inotify_mgr_t *im)
{
	if (!im) return true;
	if (!im->m_wfd_tree) return true;
	if (!im->m_wfd_tree->m_cur_node) return true;
	return false;
}

/* @func:
 *	处理inotify事件的回调函数
 */
void inotify_mgr_dispatch(inotify_mgr_t *im, char *buf, size_t buflen)
{
	if (!im || !buf || !buflen) return ;
	ssize_t rdlen = 0, i = 0;
	struct inotify_event *ev = NULL;
    char *new_name = NULL;
    int parent_wfd = 0;
    bool is_rename = false;
    unsigned int cookie = 0;

	while (true) {
		if (inotify_mgr_is_empty(im)) return ;

		rdlen = read(im->m_ifd, buf, buflen);
		if (rdlen < 0) {
			if (errno == EINTR) { errno = 0; continue; }
			else if (errno == EAGAIN || errno == EWOULDBLOCK) return ;
			else {
				INOTIFY_MGR_ERROR_LOG("read error, errno: %d - %s", errno, strerror(errno));
				return ;
			}
		} else if (rdlen == 0) break;

		for (i = 0; i < rdlen; ) {
			ev = (struct inotify_event*)&buf[i];
			i += sizeof(struct inotify_event) + ev->len;

            if ((ev->mask & IN_ISDIR) && (ev->mask & IN_CREATE)) {
                _inotify_mgr_add_watch(im, ev, false);
            } else if ((ev->mask & IN_DELETE_SELF)) {
            	_inotify_mgr_move_out_watch(im, ev);
             } else if ((ev->mask & IN_ISDIR) && (ev->mask & IN_MOVED_FROM)) {
                cookie = ev->cookie;
            } else if ((ev->mask & IN_ISDIR) && (ev->mask & IN_MOVED_TO)) {
                if (cookie == ev->cookie) {
                    is_rename = true; parent_wfd = ev->wd;
                    if (!(new_name = g_im_alloc(ev->len + 2))) continue;
                    snprintf(new_name, ev->len + 2, "%s/", ev->name);
                } else {
                    _inotify_mgr_add_watch(im, ev, false);
                }
            } else if (ev->mask & IN_MOVE_SELF) {
                /* 重命名，更新文件名即可 */
                if (is_rename) {
                    _inotify_mgr_rename_watch(im, ev->wd, parent_wfd, new_name);
                    is_rename = false; cookie = 0;
                } else {
                    /* 不是重名名，而是将目录移出监听目录 */
                    _inotify_mgr_move_out_watch(im, ev);
                }
            }

			im->m_func(im, ev, im->m_arg);
        }
    }
}

/* @func:
 *  获取一个绝对路径
 */
bool inotify_mgr_path_get(char *path, int path_len, inotify_mgr_t *im, struct inotify_event *ie)
{
    if (!path || path_len <= 0 || !ie || !im) return false;
    if (ie->len <= 0) return false;
    int len = path_len - 1;
    int namelen = 0, offset = 0;
	wfd_node_t wn_sample;
	wfd_node_t *wn_ret = NULL;
	rb_tree_node_t *tn_ret = NULL;

	wn_sample.m_wfd = ie->wd;

    if (ie->wd < 0) {
        snprintf(path, path_len, "%s", ie->name);
        return true;
    }

	path[path_len - 1] = '\0';
    namelen = strlen(ie->name);
    if (namelen > path_len - 1) return false;
    memcpy(&path[len - namelen], ie->name, namelen);
    len -= namelen;

    while (wn_sample.m_wfd >= 0) {
		/* 出错，没有搜索到根目录，就找不到父目录 */
		if (!(tn_ret = rb_tree_mgr_node_find(im->m_wfd_tree, (void*)&wn_sample))) goto err;
		if (!(wn_ret = (wfd_node_t*)tn_ret->m_data)) goto err;

		namelen = strlen(wn_ret->m_name);
		if (len - namelen < 0) return false;
		memcpy(&path[len - namelen], wn_ret->m_name, namelen);
		len -= namelen;
		wn_sample.m_wfd = wn_ret->m_parent_wfd;
    }

	offset = path_len - len + 1;
    memmove(path, &path[len], offset);
    return true;

err:
	while (true) {
		wn_sample.m_wfd = ie->wd;
		if (!(tn_ret = rb_tree_mgr_node_find(im->m_wfd_tree, (void*)&wn_sample))) break;
		if (!(wn_ret = (wfd_node_t*)tn_ret->m_data)) break;
		wn_sample.m_wfd = wn_ret->m_parent_wfd;
		_child_wfd_node_remove_watch(im, wn_ret);
	}
	return false;
}

/* @func:
 *	打印信息
 */
void inotify_mgr_dump(inotify_mgr_t *im, bool is_dump_wfd)
{
	if (!im) return ;

	INOTIFY_MGR_DEBUG_LOG("========");
	INOTIFY_MGR_TRACE_LOG("ifd: %d", im->m_ifd);
	INOTIFY_MGR_TRACE_LOG("wfd: %d", im->m_wfd);
	INOTIFY_MGR_TRACE_LOG("func: %p", im->m_func);
	INOTIFY_MGR_TRACE_LOG("arg: %p", im->m_arg);
	INOTIFY_MGR_TRACE_LOG("wfd_tree: %p", im->m_wfd_tree);
	if (is_dump_wfd) rb_tree_mgr_dump(im->m_wfd_tree, false);
	INOTIFY_MGR_DEBUG_LOG("========");
}

void inotify_mgr_event_dump(struct inotify_event *ev)
{
	if (!ev) return ;
	uint32_t mask = ev->mask;

	INOTIFY_MGR_DEBUG_LOG("========");
	INOTIFY_MGR_DEBUG_LOG("wd: %d, mask: %u, cookie: %u, len: %u, name: %s",
				ev->wd, ev->mask, ev->cookie, ev->len, ev->len ? ev->name : " ");
	if (IN_ISDIR & mask) INOTIFY_MGR_DEBUG_LOG("IN_ISDIR");
	if (IN_UNMOUNT & mask) INOTIFY_MGR_DEBUG_LOG("IN_UNMOUNT");
	if (IN_IGNORED & mask) INOTIFY_MGR_DEBUG_LOG("IN_IGNORED");
	if (IN_Q_OVERFLOW & mask) INOTIFY_MGR_DEBUG_LOG("IN_Q_OVERFLOW");
	if (IN_CREATE & mask) INOTIFY_MGR_DEBUG_LOG("IN_CREATE");
	if (IN_DELETE & mask) INOTIFY_MGR_DEBUG_LOG("IN_DELETE");
	if (IN_MODIFY & mask) INOTIFY_MGR_DEBUG_LOG("IN_MODIFY");
	if (IN_ACCESS & mask) INOTIFY_MGR_DEBUG_LOG("IN_ACCESS");
	if (IN_ATTRIB & mask) INOTIFY_MGR_DEBUG_LOG("IN_ATTRIB");
	if (IN_CLOSE_WRITE & mask) INOTIFY_MGR_DEBUG_LOG("IN_CLOSE_WRITE");
	if (IN_CLOSE_NOWRITE & mask) INOTIFY_MGR_DEBUG_LOG("IN_CLOSE_NOWRITE");
	if (IN_OPEN & mask) INOTIFY_MGR_DEBUG_LOG("IN_OPEN");
	if (IN_MOVED_FROM & mask) INOTIFY_MGR_DEBUG_LOG("IN_MOVED_FROM");
	if (IN_MOVED_TO & mask) INOTIFY_MGR_DEBUG_LOG("IN_MOVED_TO");
	if (IN_DELETE_SELF & mask) INOTIFY_MGR_DEBUG_LOG("IN_DELETE_SELF");
	if (IN_MOVE_SELF & mask) INOTIFY_MGR_DEBUG_LOG("IN_MOVE_SELF");
	INOTIFY_MGR_DEBUG_LOG("========");
}


#if 1
#include <assert.h>
#include <sys/time.h>

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
			INOTIFY_MGR_WARN_LOG("mkdir %s error, errno: %d - %s", dir_buf, errno, strerror(errno));
			return false;
		}

	next:
		*(start + 1) = ch; start++;
	}
	return true;
}

static void* _func(inotify_mgr_t *im, struct inotify_event *ev, void *arg)
{
	if (!im || !ev) return NULL;
	char buf[PATH_MAX] = {0};
	if (ev->len <= 0) return arg;

    if (ev->len > 0) {
        inotify_mgr_path_get(buf, sizeof(buf), im, ev);
		MY_PRINTF("path: %s", buf);
    }


	MY_PRINTF("---------------------------------");
	inotify_mgr_dump(im, true);
	inotify_mgr_event_dump(ev);
	MY_PRINTF("---------------------------------");
	MY_PRINTF();
	return arg;
}

int main(int argc, char *argv[])
{
	size_t buflen =  16383 * (sizeof(struct inotify_event) + NAME_MAX);
	char *buf = malloc(buflen);
	inotify_mgr_t *im = NULL;
    const char *path = argv[1];
	pthread_t pt[5];
	size_t i = 0;
	struct timeval tv;

	rb_tree_mgr_init(NULL, NULL);
	inotify_mgr_init(NULL, NULL);
	assert((im = inotify_mgr_new(path, _func, NULL)));
	inotify_mgr_dump(im, true);

#if 1
	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
		pthread_create(&pt[i], NULL, ({
			void* _(void *arg) {
				char cmd[1000][64] = {{0}};
				char cmd_rm[128] = {0};
				char old_dir[128] = {0};
				char new_dir[128] = {0};
				int j = 0, k = 0, index = 0;
				int pt_index = (int)arg;

				for (j = 0; j < 10; j++) {
					for (k = 0; k < 100; k++) {
						gettimeofday(&tv, NULL);
						snprintf(cmd[index], sizeof(cmd[index]), "%s/%d/%d/%ld/%d/%ld/%d/%ld/%d/%ld",
								path, pt_index, j, tv.tv_usec, k, tv.tv_sec, j, tv.tv_usec, k, tv.tv_sec);
						_create_dir(cmd[index], 0755);
						index++;
					}
				}

				if (!strcmp(path, "/")) return arg;
#if 1
				if (pt_index == 0)	{
					snprintf(old_dir, sizeof(old_dir), "%s/%d", path, pt_index);
					snprintf(new_dir, sizeof(new_dir), "%s/%d-%d", path, pt_index, pt_index);
					rename(old_dir, new_dir);
				}
				else if (pt_index == 1)
					snprintf(cmd_rm, sizeof(cmd_rm), "rm -rf %s/%d", path, pt_index);
				else if (pt_index == 2)
					snprintf(cmd_rm, sizeof(cmd_rm), "mv %s/%d %s/%d-%d", path, pt_index, path, pt_index, pt_index);
				system(cmd_rm);
#endif
				return arg;
			}; _;}), (void*)i);
	}
#endif

	inotify_mgr_dispatch(im, buf, buflen);
	inotify_mgr_free(im);
	free(buf);

	for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++)
		pthread_join(pt[i], NULL);
	return argc;
}

#endif

