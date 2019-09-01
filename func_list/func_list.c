#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "func_list.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define _TRACE_LOG MY_PRINTF
#define _DEBUG_LOG MY_PRINTF
#define _INFO_LOG MY_PRINTF
#define _WARN_LOG MY_PRINTF
#define _ERROR_LOG MY_PRINTF

#define _ITEM_NAME_LEN 128

typedef void* (*g_alloc_t) (size_t size);
typedef void (*g_free_t) (void *ptr);

typedef struct _func_list {
    char m_name[_ITEM_NAME_LEN]; /* 名称 */
    void *m_cb; /* 回调函数 */
    struct _func_list *m_next;
} func_list_t;

static void* _malloc2calloc(size_t size);
static void _free(void *ptr);
static g_alloc_t g_alloc = _malloc2calloc;
static g_free_t g_free = _free;
static func_list_t *g_head = NULL;

static void* _malloc2calloc(size_t size)
{
	return calloc(1, size);
}

static void _free(void *ptr)
{
    if (!ptr) return ;
    free(ptr);
}

static void _set_default(void)
{
    g_head = NULL;
    g_alloc = _malloc2calloc;
    g_free = _free;
}

/* @func:
 *  销毁管理器
 */
void func_list_destroy(void)
{
    if (!g_head) return ;
    func_list_t *fl = NULL;

    for ( ; g_head; g_head = fl) {
        fl = g_head->m_next;
        g_free(g_head);
    }
    _set_default();
}

/* @func:
 *  设置函数列表
 */
bool func_list_add(const char *name, void* cb)
{
    if (!name || !cb) return false;
    func_list_t *fl = NULL;

    if (fl = g_alloc(sizeof(*fl)), !fl) {
        _ERROR_LOG("g_alloc error, errno: %d - %s", errno, strerror(errno));
        return false;
    }

    snprintf(fl->m_name, sizeof(fl->m_name), "%s", name);
    fl->m_cb = cb;
    fl->m_next = g_head;
    g_head = fl;
    return true;
}

/* @func:
 *  获取跳转表指针
 */
void* func_list_get(const char *name)
{
    if (!g_head || !name) return NULL;
    func_list_t *fl = NULL;

    for (fl = g_head; fl; fl = fl->m_next) {
        if (strcasecmp(name, fl->m_name)) continue;
        return fl->m_cb;
    }
    return NULL;
}

/* @func:
 *  打印信息
 */
void func_list_dump(void)
{
    func_list_t *fl = NULL;

    _TRACE_LOG("===============");
    _TRACE_LOG("g_head: %p", g_head);
    _TRACE_LOG();

    for (fl = g_head ; fl; fl = fl->m_next) {
        _TRACE_LOG("++++++++++++++");
        _TRACE_LOG("name: %s", fl->m_name);
        _TRACE_LOG("cb: %p", fl->m_cb);
        _TRACE_LOG("next: %p", fl->m_next);
        _TRACE_LOG("++++++++++++++");
        _TRACE_LOG();
    }
    _TRACE_LOG("===============");
}

#if 1

#include <assert.h>

typedef void (*print_t)(void);

void print_1(void)
{
    printf("print_1\n");
}

void print_2(void)
{
    printf("print_2\n");
}

int main(void)
{
    print_t p1 = NULL, p2 = NULL, p3 = NULL;
    const char *name_1 = "cb_1";
    const char *name_2 = "cb_2";
    const char *name_3 = "cb_3";

    assert(!(p3 = (print_t)func_list_get(name_3)));
    assert(func_list_add(name_1, (void*)print_1));
    assert(func_list_add(name_2, (void*)print_2));

    assert((p1 = (print_t)func_list_get(name_1)));
    assert((p2 = (print_t)func_list_get(name_2)));
    assert(!(p3 = (print_t)func_list_get(name_3)));
    func_list_dump();
    p1(); p2();

    func_list_destroy();
    return 0;
}

#endif
