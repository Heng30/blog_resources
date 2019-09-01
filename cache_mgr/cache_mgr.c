#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <pthread.h>

#include "cache_mgr.h"

#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)
#define CM_TRACE_LOG MY_PRINTF
#define CM_DEBUG_LOG MY_PRINTF
#define CM_INFO_LOG MY_PRINTF
#define CM_WARN_LOG MY_PRINTF
#define CM_ERROR_LOG MY_PRINTF 

static void* _malloc2calloc(size_t size);
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static cache_mgr_t *g_cm = NULL;
static int g_cache_max = 32; /* 全局数组的大小 */
static int g_cm_index = 0; /* 当前g_cm数组使用的个数 */
static bool g_using_mutex = false;
static cache_mgr_alloc_t g_cm_alloc = _malloc2calloc;
static cache_mgr_free_t g_cm_free = free;

static void* _malloc2calloc(size_t size)
{
    return calloc(1, size);
}

static void _set_default(void)
{
    g_cm = NULL;
    g_cm_index = 0;
    g_cache_max = 32;
    g_cm_alloc = _malloc2calloc;
    g_cm_free = free;
    g_using_mutex = false;
}

/* @func
 *  初始化管理器
 */
bool cache_mgr_init(int cache_max, bool is_using_mutex, cache_mgr_alloc_t alloc, cache_mgr_free_t dealloc)
{
    if (cache_max <= 0) g_cache_max = 32;
    else g_cache_max = cache_max;

    if (!alloc || !dealloc) g_cm_alloc = _malloc2calloc, g_cm_free = free;
    else g_cm_alloc = alloc, g_cm_free = dealloc;

    g_using_mutex = is_using_mutex;

    if (g_cm = g_cm_alloc(sizeof(cache_mgr_t) * g_cache_max), !g_cm) {
        CM_ERROR_LOG("g_cm_alloc error, errno: %d - %s", errno, strerror(errno));
        goto err;
    }
    return true;

err:
    _set_default();
    return false;
}

/* @func:
 *  销毁管理器
 * @warn:
 *  即使有的管理器引用数不为0，这里还是会释放整个管理器的内存，未回收的内存会泄露
 */
void cache_mgr_destroy(void)
{
    if (!g_cm) return ;
    int i = 0;
    for (i = 0; i < g_cm_index; i++) cache_mgr_free(i);

    if (g_using_mutex) pthread_mutex_lock(&g_mutex);
    g_cm_free(g_cm); _set_default();
    if (g_using_mutex) pthread_mutex_destroy(&g_mutex);
}

/* @func: 
 *  创建一个管理器
 */
int cache_mgr_new(size_t size, size_t cnt)
{
   if (g_cm_index >= g_cache_max || g_cm_index < 0 || !size || !g_cm) return -1;
   int index = 0;

   if (g_using_mutex) pthread_mutex_lock(&g_mutex);
   if (size < sizeof(void*)) size = sizeof(void*); /* 缓存的大小至少要能存储一个指针大小的数据，用于链接cache */
   g_cm[g_cm_index].m_size = size;
   g_cm[g_cm_index].m_cnt = cnt;
   g_cm[g_cm_index].m_index = 0;
   g_cm[g_cm_index].m_cache = NULL;
   g_cm[g_cm_index].m_ref = 0;
   index = g_cm_index++;
   if (g_using_mutex) pthread_mutex_unlock(&g_mutex);
   return index;
}

/* @func:
 *  销毁一个管理器
 * @warn:
 *  引用不为0时，不会擦除管理器状态
 */
void cache_mgr_free(int id)
{
    if (id < 0 || id >= g_cm_index || !g_cm) return ; 
    void *cur = NULL, *next = NULL;

    if (g_using_mutex) pthread_mutex_lock(&g_mutex);
    for (cur = g_cm[id].m_cache; cur; cur = next) {
        next = *(void**)cur;
        g_cm[id].m_ref--;
        g_cm_free(cur);
    }
    g_cm[id].m_cache = NULL;
    // MY_PRINTF("ref: %d", (int)g_cm[id].m_ref);
    if (g_cm[id].m_ref == 0) memset(&g_cm[id], 0, sizeof(g_cm[id]));
    if (g_using_mutex) pthread_mutex_unlock(&g_mutex);
}

/* func:
 *  获取一个缓存
 */
void* cache_mgr_get(int id)
{
    if (id < 0 || id >= g_cm_index || !g_cm) return NULL;
    void *next = NULL, *ptr = NULL;

    if (g_using_mutex) pthread_mutex_lock(&g_mutex);
    if (!g_cm[id].m_cache) {
       if (ptr = g_cm_alloc(g_cm[id].m_size), !ptr) {
           CM_WARN_LOG("g_cm_alloc error, errno: %d - %s", errno, strerror(errno));
       } else g_cm[id].m_ref++;
    } else {
        ptr = g_cm[id].m_cache;
        next = *(void**)ptr;
        g_cm[id].m_cache = next;
        g_cm[id].m_index--;
    }
    if (g_using_mutex) pthread_mutex_unlock(&g_mutex);

    if (ptr) memset(ptr, 0, g_cm[id].m_size);
    return ptr;
}

/* @func:
 *  归还一个缓存
 * @warn:
 *  必须确保ptr是从cache_mgr_get中获取的缓存
 */
void cache_mgr_ret(int id, void *ptr)
{
    if (!ptr || id < 0 || id >= g_cm_index || !g_cm) return ;

    if (g_using_mutex) pthread_mutex_lock(&g_mutex);
    if (g_cm[id].m_index >= g_cm[id].m_cnt) {
        g_cm_free(ptr);
        g_cm[id].m_ref--;
    } else {
        *(void**)ptr = g_cm[id].m_cache;
        g_cm[id].m_cache = ptr;
        g_cm[id].m_index++;
    }
    if (g_using_mutex) pthread_mutex_unlock(&g_mutex);
}

/* =======================Test==================== */
#if 1
#include <assert.h>

int main()
{
    int id_1 = 0, id_2 = 0, id_3 = 0;
    char *ptr_1 = NULL, *ptr_2 = NULL, *ptr_3 = NULL;
    char *ptr_1_1 = NULL, *ptr_2_1 = NULL;
    char *ptr_1_2 = NULL, *ptr_2_2 = NULL;
    const char *str = "hello world";

    assert(cache_mgr_init(2, true, NULL, NULL));
    assert((id_1 = cache_mgr_new(16, 2), id_1 >= 0));
    assert((id_2 = cache_mgr_new(32, 2), id_2 >= 0));
    assert((id_3 = cache_mgr_new(64, 2), id_3 < 0));
    
    assert((ptr_1 = cache_mgr_get(id_1), ptr_1));
    assert((ptr_2 = cache_mgr_get(id_2), ptr_2));
    assert((ptr_3 = cache_mgr_get(id_3), !ptr_3));
    assert((ptr_1_1 = cache_mgr_get(id_1), ptr_1_1));
    assert((ptr_2_1 = cache_mgr_get(id_2), ptr_2_1));

    memcpy(ptr_1, str, strlen(str));
    memcpy(ptr_1_1, str, strlen(str));
    memcpy(ptr_2, str, strlen(str));
    memcpy(ptr_2_1, str, strlen(str));

    cache_mgr_ret(id_1, ptr_1);
    cache_mgr_ret(id_1, ptr_1_1);
    cache_mgr_ret(id_2, ptr_2_1);
    cache_mgr_ret(id_2, ptr_2);

    assert((ptr_1_2 = cache_mgr_get(id_1), ptr_1_2));
    assert((ptr_2_2 = cache_mgr_get(id_2), ptr_2_2));
    assert(ptr_1_1 == ptr_1_2);
    assert(ptr_2 == ptr_2_2);

    cache_mgr_free(id_1);
    cache_mgr_ret(id_1, ptr_1_2);
    cache_mgr_ret(id_2, ptr_2_2);
    cache_mgr_destroy();

    MY_PRINTF("OK");
    return 0;
}
#endif
