/* @desc:
 *  1. ���̰߳�ȫ
 *  2. �ڴ��������û���Զ��ڴ���գ���Ҫ�ֶ����ý����ڴ����
 *  3. �ڴ����һ��ϵͳ���䣬��η���
 *  4. ͨ��������ʽά�������ڴ�
 */

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#include "memory_pool_mgr.h"

#define MY_PRINT(format, ...) printf(format"\n", ##__VA_ARGS__)
#define MEMORY_POOL_MGR_TRACE_LOG MY_PRINT
#define MEMORY_POOL_MGR_DEBUG_LOG MY_PRINT
#define MEMORY_POOL_MGR_INFO_LOG MY_PRINT
#define MEMORY_POOL_MGR_WARN_LOG MY_PRINT
#define MEMORY_POOL_MGR_ERROR_LOG MY_PRINT

#define MEMORY_POOL_MGR_MAGIC_NUM 0XAA

typedef struct _memory_pool_mgr {
    unsigned short m_index; /* �����±� */
	size_t m_size;	/* chunk ��С */
    size_t m_used;	/* ����ʹ�õ�chunk���� */
	size_t m_allocated;	/* һ�������˶��ٸ�chunk */
	void *m_free_list; /* free buffer list */
	pthread_mutex_t m_mutex; /* freeList�Ļ����� */
} memory_pool_mgr_t;

/* ÿһƬ�ڴ�ǰ����ά��һ�����������ݽṹ��
 * ���ڿ����ҵ���Ӧ�Ĺ�����
 */
typedef struct _hdr {
    unsigned short m_index; /* �±� */
    bool m_is_raw; /* �Ƿ��ǲ����ڹ�����������ڴ� */
    unsigned char m_magic_num; /* ħ���� */
} hdr_t;

static memory_pool_mgr_t *g_head = NULL; /* �ڴ�ع�����ͷ�ڵ� */
static unsigned short g_member_size = 0; /* pool_mgr�Ĵ�С */
static size_t g_start_size = 0; /* ��һ���������Ĵ�С */
static size_t g_step_size = 0; /* ���ڹ������Ĵ�С��� */
static size_t g_max_size = 0; /* ���һ���������Ĵ�С */

/* @func:
 *  ��ӡȫ����Ϣ
 */
static void _memory_pool_mgr_global_dump(void)
{
    MEMORY_POOL_MGR_TRACE_LOG("==========");
    MEMORY_POOL_MGR_TRACE_LOG("g_head: %p", g_head);
    MEMORY_POOL_MGR_TRACE_LOG("g_member_size: %d", g_member_size);
    MEMORY_POOL_MGR_TRACE_LOG("g_max_size: %lu", g_max_size);
    MEMORY_POOL_MGR_TRACE_LOG("g_start_size: %lu", g_start_size);
    MEMORY_POOL_MGR_TRACE_LOG("g_step_size: %lu", g_step_size);
    MEMORY_POOL_MGR_TRACE_LOG("==========");

}

/* @func:
 *  ��ӡͷ��Ϣ
 */
static void _memory_pool_mgr_hdr_dump(hdr_t *hdr)
{
    if (!hdr) return ;

    MEMORY_POOL_MGR_TRACE_LOG("==========");
    MEMORY_POOL_MGR_TRACE_LOG("index: %d", hdr->m_index);
    MEMORY_POOL_MGR_TRACE_LOG("is_raw: %d", hdr->m_is_raw);
    MEMORY_POOL_MGR_TRACE_LOG("mageic_num: %#x", (unsigned char)hdr->m_magic_num);
    MEMORY_POOL_MGR_TRACE_LOG("==========");
}


/* @func:
 *      ���������ڴ�ĺ��� 
 */
static void* _memory_pool_mgr_alloc(memory_pool_mgr_t *mm)
{
	if (!mm) return NULL;
	void *ptr = NULL;

    if (!(ptr = malloc(mm->m_size + sizeof(hdr_t)))) {
        MEMORY_POOL_MGR_WARN_LOG("malloc error, errno: %d - %s", errno, strerror(errno));
        return NULL;
	}

	__sync_add_and_fetch(&mm->m_allocated, 1);
	__sync_add_and_fetch(&mm->m_used, 1);
	return ptr;
}

/* @func:
 *      ���Ȼ�ȡ�ڴ���еĿ����ڴ棬û������ϵͳ��������ڴ�
 */
static void* _memory_pool_mgr_get(memory_pool_mgr_t *mm)
{
	if (!mm) return NULL;
	void *ptr = NULL; 
		
	if (!(ptr = mm->m_free_list)) {
		ptr = _memory_pool_mgr_alloc(mm);
	} else {
		mm->m_free_list = *(void**)mm->m_free_list;
		__sync_add_and_fetch(&mm->m_used, 1);
	}

	return ptr;
}

/* @func:
 *      ���ڴ淵�����ڴ��
 */
static bool _memory_pool_mgr_free(memory_pool_mgr_t *mm, void *ptr)
{
    if (!ptr || !mm) return false;

    *(void**)ptr = (void*)mm->m_free_list;
    mm->m_free_list = ptr;          
    __sync_sub_and_fetch(&mm->m_used, 1);

    return true;          
}

/* @func:
 *      ��ʼ������ʹ�õ��ڴ��
 * @param:
 *  start_size: ��һ����������chunk��С
 *  step_size: ÿ��������chunk�Ĵ�С���
 *  member_size: ������������
 */
bool memory_pool_mgr_init(size_t start_size, size_t step_size, unsigned short member_size)
{
    if (member_size >= USHRT_MAX) member_size = USHRT_MAX - 1;
    if (start_size == 0) start_size = 16;
    if (step_size == 0) step_size = 16;
    unsigned short i = 0;

    if (!(g_head = (memory_pool_mgr_t*)calloc(member_size, sizeof(memory_pool_mgr_t)))) {
        MEMORY_POOL_MGR_ERROR_LOG("calloc error, errno: %d - %s", errno, strerror(errno));
        return false;
    }

    for (i = 0; i < member_size; i++) {
        if (pthread_mutex_init(&g_head[i].m_mutex, NULL)) {
            MEMORY_POOL_MGR_ERROR_LOG("pthread_mutex_init error, errno: %d - %s", errno, strerror(errno));
            goto err; 
        }

        g_head[i].m_size = start_size + step_size * i;
        g_head[i].m_index = i;
        g_member_size = i + 1;
    }
    
    g_step_size = step_size;
    g_start_size = start_size;
    g_max_size = g_start_size + g_step_size * (g_member_size - 1);
    return true;

err:
    memory_pool_mgr_destroy();
    return false;
}

/* @func:
 *  ����һ���������Ŀ����ڴ�
 */
static void _memory_pool_mgr_flush(memory_pool_mgr_t *mm)
{
    if (!mm) return ;
	void *tmp = NULL, *next = NULL;

    for (tmp = mm->m_free_list; tmp; tmp = next) {		
        next = *(void**)tmp;
        __sync_sub_and_fetch(&mm->m_allocated, 1);
        free(tmp);
    }
    mm->m_free_list = NULL;
}

/* @func:
 *      ����ȫ���ڴ�أ����ڳ����˳�ʱʹ��
 */
void memory_pool_mgr_destroy(void)
{
	if (!g_head) return ;
    unsigned short i = 0;
	
    for (i = 0; i < g_member_size; i++) {
        pthread_mutex_lock(&g_head[i].m_mutex);
        _memory_pool_mgr_flush(&g_head[i]);
        pthread_mutex_unlock(&g_head[i].m_mutex);

        pthread_mutex_destroy(&g_head[i].m_mutex);
    }

    free(g_head);
    g_head = NULL;
}

/* @func:
 *      ������л���صĿ����ڴ�
 */
void memory_pool_mgr_gc(void)
{
    if (!g_head) return ;
    unsigned short i = 0;

	for (i = 0; i < g_member_size; i++) {
        pthread_mutex_lock(&g_head[i].m_mutex);
        _memory_pool_mgr_flush(&g_head[i]);
        pthread_mutex_unlock(&g_head[i].m_mutex);
	}
}

/* @func:
 *  ����һ��������
 */
static memory_pool_mgr_t *_memory_pool_mgr_find(size_t size)
{
    unsigned short index = 0;

    if (!g_head) return NULL;
    if (size > g_max_size) return NULL;
    if (size <= g_start_size) return &g_head[0]; 

    index = ((size - g_start_size) +  (g_step_size - 1))  / g_step_size;
    if (index >= g_member_size) return NULL;
    return &g_head[index];
}

/* @func:
 *  ����һ��hdr
 */
static void _memory_pool_mgr_hdr_set(void *ptr, bool is_raw, unsigned short index)
{
    if (!ptr) return ;
    hdr_t *hdr = (hdr_t*)ptr;
    memset(hdr, 0, sizeof(hdr_t));
    
    hdr->m_index = index;
    hdr->m_is_raw = is_raw;
    hdr->m_magic_num = MEMORY_POOL_MGR_MAGIC_NUM;
}

/* @func:
 *  ͨ���ڴ������
 *  �ڴ��С�����ڴ�ط�Χ������ϵͳ���룬�ͷŵ�ʱ�򷵻���ϵͳ
 */
void* memory_pool_mgr_alloc(size_t size)
{
    if (!size) return NULL;
    memory_pool_mgr_t *mm = NULL;
    void *ptr = NULL;
    bool is_raw = false;
    unsigned short index = 0;

    if ((mm = _memory_pool_mgr_find(size))) {
        pthread_mutex_lock(&mm->m_mutex);
        ptr = _memory_pool_mgr_get(mm);
        pthread_mutex_unlock(&mm->m_mutex);
        if (!ptr) return NULL;
    } else {
        if (!(ptr = malloc(size + sizeof(hdr_t)))) {
            MEMORY_POOL_MGR_WARN_LOG("malloc error, errno: %d - %s", errno, strerror(errno));
            return NULL;
        }
        is_raw = true;
    }

    if (is_raw) memset(ptr, 0, size + sizeof(hdr_t));
    else memset(ptr, 0, mm->m_size + sizeof(hdr_t));

    index = mm ? mm->m_index : USHRT_MAX;
    _memory_pool_mgr_hdr_set(ptr, is_raw, index);
    ptr = (void*)((char*)ptr + sizeof(hdr_t));
    return ptr;
}

/* @func:
 *		ͨ���ڴ�����
 */
void memory_pool_mgr_free(void *ptr)
{
	if (!ptr) return ;
    hdr_t *hdr = (hdr_t*)((char*)ptr - sizeof(hdr_t));
    memory_pool_mgr_t *mm = NULL;

    if (!g_head) goto free_exit;
    if (hdr->m_is_raw) goto free_exit;
    if (hdr->m_magic_num != MEMORY_POOL_MGR_MAGIC_NUM) goto free_exit;
    if (hdr->m_index >= g_member_size) goto free_exit;

    mm = &g_head[hdr->m_index];
    pthread_mutex_lock(&mm->m_mutex);
    _memory_pool_mgr_free(mm, hdr);
    pthread_mutex_unlock(&mm->m_mutex);
    return ;

free_exit:
    free(hdr);
}

/* @func:
 *      ��ӡͳ����Ϣ
 */
static void _memory_pool_mgr_dump(memory_pool_mgr_t *mm)
{
    if (!mm) return ;
	
    MEMORY_POOL_MGR_TRACE_LOG("================");
    MEMORY_POOL_MGR_TRACE_LOG("index %u", mm->m_index);
    MEMORY_POOL_MGR_TRACE_LOG("chunk_size: %lu", mm->m_size); 
    MEMORY_POOL_MGR_TRACE_LOG("allocated: %lu", mm->m_allocated); 
    MEMORY_POOL_MGR_TRACE_LOG("used: %lu", mm->m_used); 
    MEMORY_POOL_MGR_TRACE_LOG("chunk_size: %lu", mm->m_size); 
    MEMORY_POOL_MGR_TRACE_LOG("free_list: %p", mm->m_free_list); 
    MEMORY_POOL_MGR_TRACE_LOG("================");
}

/* @func:
 *      ��ӡ����ͳ����Ϣ
 */
void memory_pool_mgr_dump(void)
{
    if (!g_head) return ;
    unsigned short i = 0;
    unsigned long long allocated = 0, used = 0;

    for (i = 0; i < g_member_size; i++) {
        _memory_pool_mgr_dump(&g_head[i]);
        allocated += g_head[i].m_size * g_head[i].m_allocated;
        used += g_head[i].m_size * g_head[i].m_used;
    }

    MEMORY_POOL_MGR_TRACE_LOG("================");
    MEMORY_POOL_MGR_TRACE_LOG("allcated: %lld", allocated);
    MEMORY_POOL_MGR_TRACE_LOG("used: %lld", used);
    MEMORY_POOL_MGR_TRACE_LOG("================");
}


static void* _memory_pool_mgr_test(void *arg)
{
    char *str = NULL; 
    int i = 0, j = 0;
    size_t test_count = 10;
    
    for (i = 0; i < test_count ; i++) {
        for (j = 1; j < 65535; j++) {
            str = (char*)memory_pool_mgr_alloc(j);
            if (str) memcpy(str, "h", 1);
            else MY_PRINT("alloc error");
            memory_pool_mgr_free(str);
        }

        if (i % 100 == 0) memory_pool_mgr_gc();
    }

    return NULL;
}

int main(void)
{
    pthread_t pt[10];
    unsigned short member_size = 65535;
    size_t start_size = 16;
    size_t step_size = 10;
    int i = 0;

    if (!memory_pool_mgr_init(start_size, step_size, member_size)) {
        MY_PRINT("init error");
        return -1;
    }

    for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
        pthread_create(&pt[i], NULL, _memory_pool_mgr_test, NULL);
    }
   
    for (i = 0; i < sizeof(pt) / sizeof(pt[0]); i++) {
        pthread_join(pt[i], NULL);
    }
    
    memory_pool_mgr_dump();
    memory_pool_mgr_gc();
    MY_PRINT("\n");
    memory_pool_mgr_dump();
    memory_pool_mgr_destroy();
    return 0;
}
