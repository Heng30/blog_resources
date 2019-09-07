/* @func:
 *	其他一些通用的函数
 */
#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h> 
#include <stdbool.h>


/* @func:
 *	获取格式化时间
 */
char *get_localtime_format(void);

/* @func:
 *	获取线程id
 */
pid_t gettid();

/* @func:
 *	查找内存中的字符串
 */
const char* memstr(const char* buf, size_t buflen, const char* substr);

/* @func:
 *	获取两个字符串间的字符串
 */
bool get_str_between(char *buf, size_t buf_len, const char *raw_str, const char *str_s, const char *str_e);


/* @func:
 *	大小写互转
 */
void AtoaORatoA(const char *key, char *tmp, int key_len);

/* @func:
 *		分割ip:port
 */
int get_server_ipport(char *ipport,char *ip);

/* @func:
 *	后台运行模式
 */
int run_as_daemon(void);

unsigned char *base64_decode(const unsigned char *str, int strict);

char* base64_encode(const unsigned char *value, int vlen);

/* @func:
 *      获取系统内核信息
 */
const char* get_kernel_info(void);

/* @func:
 *	调用系统命令
 */
bool pox_system(const char *cmd);

/* @func:
 *	查找有多少个子串
 */
size_t substr_count(const char *str, size_t str_len, const char *substr);

/* @func:
 *	替换一个字符串
 */
const char* replace_str(const char *buf, size_t len, const char *addr_old, const char *addr_new);

/* @func:
 *	替换字符串
 */
const char* replace_str_all(const char *buf, size_t len, const char *addr_old, const char *addr_new);

bool u2g(char *inbuf, size_t inlen,char *outbuf, size_t* outlen);

bool g2u(char *inbuf, size_t inlen, char *outbuf, size_t *outlen);

/* @func:
 *	复制一个文件
 */
bool copy_file(const char *src, const char *dst);

/* @func:
 *	读取套接字内容
 */
ssize_t read_all(int fd, char *buf, size_t len);

/* @func:
 *	写入套接字
 */
ssize_t write_all(int fd, const char *buf, size_t len);

/* @func:
 *	加载整个文件内容到内存中
 */
const unsigned char* load_file(const char *path);

#endif
