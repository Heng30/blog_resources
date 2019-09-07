#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h> 
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/utsname.h>
#include <iconv.h>
#include <errno.h>
#include <stdbool.h>

#include "utility.h"
#include "logger.h"
#include "common2.h"

#define UTILITY_WARN_LOG WARN_C_LOG
#define UTILITY_INFO_LOG INFO_C_LOG
#define UTIITY_DEBUG_LOG DEBUG_C_LOG

/* @func:
 *	获取线程id
 */
pid_t gettid()
{
	return syscall(SYS_gettid); 
}

/* @func:
 *      获取系统内核信息
 */
const char* get_kernel_info(void)
{
    struct utsname buf;
    static char info[326] = {0};
    if (info[0]) return info;
    if (uname(&buf)) {
        UTILITY_WARN_LOG("Crazy! uname error, errno: %d - %s", errno, strerror(errno));
        return NULL;
    }
    snprintf(info, sizeof(info), "%s %s %s %s %s", buf.sysname, buf.nodename, buf.release, buf.version, buf.machine);
    return info;
}


/* @func:
 *	获取格式化时间
 */
char *get_localtime_format(void)
{
	struct timeval tv;
	struct tm *p = NULL;
	static char str[256];

	gettimeofday(&tv, NULL);
	p = localtime(&tv.tv_sec);
	snprintf(str, sizeof(str), "%04d-%02d-%02d %02d:%02d:%02d",
		1900 + p->tm_year,
		1 + p->tm_mon,
		p->tm_mday,
		p->tm_hour,
		p->tm_min,
		p->tm_sec);
	
	return str;
}


/* @func:
 *	查找内存中的字符串
 */
const char* memstr(const char* buf, size_t buflen, const char* substr)
{
	if (NULL == buf || 0 >= buflen || NULL == substr || '\0' == *substr) {
		return NULL;
	}
	
	int sublen = strlen(substr);
	int i = 0;
	const char* cur = buf;
	int endpos = buflen - sublen + 1;
	
	for (i = 0; i < endpos; i++) {
		if (*cur == *substr) {
			if (0 == memcmp(cur, substr, sublen)) {
				return cur;
			}
		}
		cur++;
	}
	return NULL;
}

/* @func:
 *	获取两个字符串间的字符串
 */
bool get_str_between(char *buf, size_t buf_len, const char *raw_str, const char *str_s, const char *str_e) 
{
	if (!raw_str || !buf || !str_s || !str_e) return false;

	const char *ptr_s = NULL, *ptr_e = NULL;
	if (!(ptr_s = strstr(raw_str, str_s))) return false;
	ptr_s += strlen(str_s);
	if (!(ptr_e = strstr(ptr_s, str_e))) return false;
	strncpy(buf, ptr_s, MIN(ptr_e - ptr_s, buf_len - 1));
	buf[buf_len - 1] = '\0';
	return true;
}


/* @func:
 *	大小写互转
 */
void AtoaORatoA(const char *key, char *buf, int buflen)
{	
	const char *a = key;
	int i = 0;
	while(*a && i < buflen){
		if(*a >= 'a' && *a <= 'z'){
			*buf -= 32;
		} else if (*a >= 'A' && *a <= 'Z'){
			*buf += 32;
		}
		a++; buf++; i++;
	}
}

/* @func:
 *		分割ip:port
 */
int get_server_ipport(char *ipport,char *ip)
{
	if(!*ipport) return -1;
	int port = 0;
	char tmp[64]= {0};
	int n = 0;
		
	sprintf(tmp,"%s",ipport);
	UTIITY_DEBUG_LOG("serverIp-port: %s\n",tmp);
	char *p = strrchr(tmp,':');
	if(p) n = p-tmp;
	if(++p) port = atoi(p);
	memcpy(ip,tmp,n);
	ip[n] = '\0';
	return port;	
}

/* @func:
 *	后台运行模式
 */
int run_as_daemon(void)
{
	int fd = 0;
	switch (fork()) {
		case -1: {
			UTILITY_WARN_LOG("fork error, errno: %d - %s\n", errno, strerror(errno));
			return -1;
		}
		case 0: break;
		default: exit(EXIT_SUCCESS);
	}

	if (setsid() == -1) {
		UTILITY_WARN_LOG("setsid error, errno: %d - %s\n", errno, strerror(errno));
		return -1;
	}

	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGSTOP, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGKILL, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGABRT, SIG_IGN);
	
	if ((fd = open("/dev/null", O_RDWR, 0)) >= 0) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
		if (fd > 2) close (fd);
	}
	
	return 0;
}


unsigned char *base64_decode(const unsigned char *str, int strict)
{
	if (!str) return NULL;
	
	static const char base64_pad = '=';
	static const short base64_reverse_table[256] = {
		-2, -2, -2, -2, -2, -2, -2, -2, -2, -1, -1, -2, -2, -1, -2, -2,
		-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
		-1, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, 62, -2, -2, -2, 63,
		52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -2, -2, -2, -2, -2, -2,
		-2,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
		15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -2, -2, -2, -2, -2,
		-2, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
		41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -2, -2, -2, -2, -2,
		-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
		-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
		-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
		-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
		-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
		-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
		-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
		-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2
	};
		
	const unsigned char *current = str;
	int length = strlen((const char*)str);
	int ch, i = 0, j = 0, k;
	
	unsigned char *result;
	
	result = (unsigned char *)malloc(length + 1);

	while ((ch = *current++) != '\0' && length-- > 0) {
		if (ch == base64_pad) {
			if (*current != '=' && (i % 4) == 1) {
				free(result);
				return NULL;
			}
			continue;
		}

		ch = base64_reverse_table[ch];
		if ((!strict && ch < 0) || ch == -1) { 
			continue;
		} else if (ch == -2) {
			free(result);
			return NULL;
		}

		switch(i % 4) {
		case 0:
			result[j] = ch << 2;
			break;
		case 1:
			result[j++] |= ch >> 4;
			result[j] = (ch & 0x0f) << 4;
			break;
		case 2:
			result[j++] |= ch >>2;
			result[j] = (ch & 0x03) << 6;
			break;
		case 3:
			result[j++] |= ch;
			break;
		}
		i++;
	}

	k = j;
	
	if (ch == base64_pad) {
		switch(i % 4) {
		case 1:
			free(result);
			return NULL;
		case 2:
			k++;
		case 3:
			result[k++] = 0;
		}
	}

	result[j] = '\0';
	return result;
}

char *base64_encode(const unsigned char *value, int vlen)
{
	if (!value || vlen < 0) return NULL;
	static char basis_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; 
	unsigned char oval = 0 ;  
    char *result = (char *)malloc((vlen * 4) / 3 + 5) ; 
    char *out = result; 
	
	while (vlen >= 3) { 
		*out++ = basis_64[value[0] >> 2]; 
		*out++ = basis_64[((value[0] << 4) & 0x30) | (value[1] >> 4)]; 
		*out++ = basis_64[((value[1] << 2) & 0x3C) | (value[2] >> 6)]; 
		*out++ = basis_64[value[2] & 0x3F]; 
		value += 3; 
		vlen -= 3; 
	}
	
	if (vlen > 0) { 
		*out++ = basis_64[value[0] >> 2]; 
		oval = (value[0] << 4) & 0x30 ; 
		if (vlen > 1) oval |= value[1] >> 4; 
		*out++ = basis_64[oval]; 
		*out++ = (vlen < 2) ? '=' : basis_64[(value[1] << 2) & 0x3C]; 
		*out++ = '='; 
	} 
	*out = '\0';  
	return result; 
} 


/* @func:
 *	调用系统命令
 */
typedef void (*sighandler_t)(int);  
bool pox_system(const char *cmd)
{  
	if (!cmd) return false;
	
	sighandler_t old_handler;  
	bool ret = true;
	int status = 0;

	old_handler = signal(SIGCHLD, SIG_DFL);
	status = system(cmd);
	if (-1 == status) {
		UTILITY_WARN_LOG("system error, errno: %d - %s, cmd: %s", errno, strerror(errno), cmd);
		ret = false;
	}
	signal(SIGCHLD, old_handler);  

	return ret;  
} 

/* @func:
 *	替换一个字符串
 */
const char* replace_str(const char *buf, size_t len, const char *addr_old, const char *addr_new)
{
	if (!buf || !addr_new || !addr_old) return NULL;
	
	int len_diff = strlen(addr_new) - strlen(addr_old);
	size_t new_len = 0, offset = 0;
	char *new_buf = NULL, *new_buf_start = NULL;
	const char *ptr_s = NULL, *ptr_e = NULL;
	
	if (len_diff < 0 && abs(len_diff) > len) return NULL;
	new_len = len + len_diff;

	if (!(new_buf = (char*)calloc(1, new_len + 1))) {
		UTILITY_WARN_LOG("calloc error, errno: %d - %s", errno, strerror(errno));
		return NULL;
	}
	ptr_s = buf; 
	new_buf_start = new_buf;
	
	if (!(ptr_e = memstr(ptr_s, len, addr_old))) return NULL;
	offset = ptr_e - ptr_s;
	memcpy(new_buf, ptr_s, offset);
	new_buf += offset;
	
	offset = strlen(addr_new);
	memcpy(new_buf, addr_new, offset);
	new_buf += offset;
	ptr_s = ptr_e + strlen(addr_old);

	offset = buf + len - ptr_s;
	memcpy(new_buf, ptr_s, offset);	
	new_buf_start[new_len] = '\0';

	return new_buf_start;
}


/* @func:
 *	查找有多少个子串
 */
size_t substr_count(const char *str, size_t str_len, const char *substr)
{
	if (!str || !substr) return 0;
	size_t count = 0, len = 0;
	const char *start = str;
	int left_len = str_len;
	len = strlen(substr);
	
	while (left_len > 0 && (str = memstr(start, left_len, substr))) {
		left_len = left_len - (str - start) - len;
		start = str + len;
		count++;
	}
	return count;
}

/* @func:
 *	替换字符串
 */
const char* replace_str_all(const char *buf, size_t len, const char *addr_old, const char *addr_new)
{
	if (!buf || !addr_new || !addr_old) return NULL;

	int len_diff = strlen(addr_new) - strlen(addr_old);
	size_t new_len = 0, offset = 0;
	char *new_buf = NULL, *new_buf_start = NULL;
	const char *ptr_s = NULL, *ptr_e = NULL;
	size_t count = 0;
	int str_len = len;
	int addr_old_len = strlen(addr_old);
	int addr_new_len = strlen(addr_new);

	if (addr_new_len == addr_old_len && 
		!strcmp(addr_new, addr_old)) return NULL;
	
	if (len_diff < 0 && abs(len_diff) > len) return NULL;

	if (0 == (count = substr_count(buf, len, addr_old))) return NULL;
	new_len = len + len_diff * count;
	
	if (!(new_buf = (char*)calloc(1, new_len + 1))) {
		UTILITY_WARN_LOG("calloc error, errno: %d - %s", errno, strerror(errno));
		return NULL;
	}
	ptr_s = buf; 
	new_buf_start = new_buf;
	
	while (str_len > 0 && (ptr_e = memstr(ptr_s, str_len, addr_old))) {
		offset = ptr_e - ptr_s;
		memcpy(new_buf, ptr_s, offset);
		new_buf += offset;
		str_len -= offset;
		
		memcpy(new_buf, addr_new, addr_new_len);
		new_buf += addr_new_len;
		
		ptr_s = ptr_e + addr_old_len;
		str_len -= addr_old_len;
	}
	
	offset = buf + len - ptr_s;
	memcpy(new_buf, ptr_s, offset);	
	new_buf_start[new_len] = '\0';

	return new_buf_start;
}

/* @func: 
 *	转码函数
 * 
 * @param:
 *	from_charset: 源字符串的编码格式
 *	to_charset: 目标字符串的编码格式
 *	inbuf: 源字符串
 *	inlen: 源字符串长度
 *	outbuf: 目标字符串缓冲区
 *	outlen: 目标字符串缓冲区大小
 *	
 * @return:
 *	成功：true
 *	失败：false
 * 
 * @bug:
 *  某些utf8的字符串，如"登录成功"，g2u是能够通过转换的；
 *	通过返回值判断不是一个可靠的方法，尽量避免；
 *
 */
static bool code_convert(char *from_charset, char *to_charset, 
						char *inbuf, size_t inlen, char *outbuf, size_t *outlen) {
	if (!from_charset || !to_charset || !inbuf || !outbuf) return false;
	
    iconv_t cd;
    char **pin = &inbuf;
    char **pout = &outbuf;
	
    if ((iconv_t)-1 == (cd = iconv_open(to_charset, from_charset))) {
		UTILITY_WARN_LOG("iconv_open error, errno: %d - %s", errno, strerror(errno));
		return false;
	}
    
	memset(outbuf, 0, *outlen);
    if (iconv(cd, pin, &inlen, pout, outlen) == (size_t)-1) {
		UTILITY_WARN_LOG("iconv error: %d - %s", errno, strerror(errno));
		iconv_close(cd);
		return false;
	}
    iconv_close(cd);
    return true;
}


bool u2g(char *inbuf, size_t inlen,char *outbuf, size_t* outlen)
{
    return code_convert("utf-8", "gbk", inbuf ,inlen, outbuf, outlen);
}

bool g2u(char *inbuf, size_t inlen, char *outbuf, size_t *outlen)
{
    return code_convert("gbk", "utf-8", inbuf, inlen, outbuf, outlen);
}

/* @func:
 *	读取套接字内容
 */
ssize_t read_all(int fd, char *buf, size_t len)
{
	if (fd < 0 || !buf || !len) return 0;
	int byte = 0;

	do {
		byte = read(fd, buf, len);
	} while (byte < 0 && errno == EINTR);

	return byte;
}

/* @func:
 *	写入套接字
 */
ssize_t write_all(int fd, const char *buf, size_t len)
{
	if (fd < 0 || !buf || !len) return 0;
	size_t total = 0;
	long long writelen = 0;

	while (len > 0) {
		writelen = write(fd, buf, len);
		if (writelen < 0) {
			if (errno == EINTR) continue;
			return writelen;
		}

		total += writelen;
		buf += writelen;
		len -= writelen;
	}
	return total;
}

/* @func:
 *	复制一个文件
 */
bool copy_file(const char *src, const char *dst)
{
	if (!src || !dst) return false;
	bool ret = true;
	int srcfd = 0, dstfd = 0;
	int readlen = 0, writelen = 0;
	char buf[1024] = {0};
	const char *ptr = buf;

	if (-1 == (srcfd = open(src, O_RDONLY))) {
		UTILITY_WARN_LOG("open %s error, errno: %d - %s", src, errno, strerror(errno));
		return false;
	}

	if (-1 == (dstfd = open(dst, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR))) {
		UTILITY_WARN_LOG("open %s error, errno: %d - %s", src, errno, strerror(errno));
		return false;
	}

	while ((readlen = read(srcfd, buf, sizeof(buf)))) {
		if (-1 == readlen && errno != EINTR) { ret = false; break; }
		else if (readlen > 0) {
			ptr = buf;
			while ((writelen = write(dstfd, ptr, readlen))) {
				if (-1 == writelen && errno != EINTR) break;
				else if (writelen == readlen) break;
				else if (writelen > 0) {
					ptr += writelen; 
					readlen -= writelen;
				}
			}

			if (-1 == writelen) { ret = false; break; }
		}
	}

	close(srcfd); close(dstfd);
	return ret;
}

/* @func:
 *	加载整个文件内容到内存中
 */
const unsigned char* load_file(const char *path)
{
	if (!path) return NULL;
	int len = 0, readlen = 0;
	unsigned char *content = NULL;;
	FILE* fp = NULL;

	if (!(fp = fopen(path, "rb"))) {
		UTILITY_WARN_LOG("fopen %s error, errno: %d - %s", path, errno, strerror(errno));
		return false;
	}

	if (fseek(fp, 0L, SEEK_END)) {
		UTILITY_WARN_LOG("fseek %s error, errno: %d - %s", path, errno, strerror(errno));
		goto err;
	}

	if (-1L == (len = ftell(fp))) {
		UTILITY_WARN_LOG("ftell %s error, errno: %d - %s", path, errno, strerror(errno));
		goto err;
	}

	if (fseek(fp, 0L, SEEK_SET)) {
		UTILITY_WARN_LOG("fseek %s error, errno: %d - %s", path, errno, strerror(errno));
		goto err;
	}

	if (!(content = (unsigned char*)malloc(len + 1))) goto err;
	readlen = fread(content, 1, len, fp);
	if (readlen != len) goto err;
	content[len] = '\0';
	return content;
	
	fclose(fp);
	return content;

err:
	if (fp) fclose(fp);
	if (content) free(content);
	return NULL;
} 


