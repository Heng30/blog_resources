#include <stdio.h>
#include <string.h>
#include <openssl/md5.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define SAMPLE_POS 3 /* 计算md5值的采集点个数 */
#define CHUNK_SIZE 512 /* 读取文件的块大小缓冲区 */
#define MD5_RAW_BUF_SIZE 16 /* md5值原始数据为16字节 */
#define MD5_HEX_BUF_SIZE (MD5_RAW_BUF_SIZE * 2) /* md5值的16进制表示形式，占用32字节 */
#define ONCE_MD5SUM_FILE_SIZE (1024 * 1024 * 10) /* 小于该大小的文件，整个文件进行MD5计算 */

/* @func:
 *		md5值转换为16进制表示形式
 *
 * @param:
 *		md5Raw: md5的原始值
 *		md5: md5的16进制表示缓冲区
 *		len: md5缓冲区的大小
 */
static void _Md5Raw2Hex(unsigned char *md5Raw, char *md5, int len)
{
	int i = 0;
	char c = 0;
	for (i = 0; i < MD5_RAW_BUF_SIZE; i++) {
		c = (md5Raw[i] >> 4) & 0x0F;
		md5[2 * i] = "0123456789ABCDEF"[c];	
		c = md5Raw[i] & 0x0F;
		md5[2 * i + 1] = "0123456789ABCDEF"[c];	
	}
}

/* @func:
 *		计算一个文件的md5值；
 *		大于ONCE_MD5SUM_FILE_SIZE的文件，进行SAMPLE_POS次采集，计算md5值
 *	
 * @param：
 *		filepath: 文件路径
 *		md5: md5的16进制表示缓冲区
 *		len: md5缓冲区的大小
 */
bool Md5Sum(const char *filepath, char *md5, int len)
{
	if (!filepath || !md5 || len < MD5_HEX_BUF_SIZE) return false;
	
	unsigned char buf[CHUNK_SIZE + 1] = {0};
	unsigned char md5Raw[MD5_RAW_BUF_SIZE] = {0};
	FILE *fp = NULL;
	struct stat st;
	off_t fsize = 0;
	MD5_CTX md5_ctx;
	int i = 0, chunkNum = 0;
	size_t rlen = 0;
	off_t pos[SAMPLE_POS] = {0}; /* 用于保存文件的偏移位置 */

	if (!(fp = fopen(filepath, "rb"))) {
		printf("fopen %s error, errno: %d - %s\n", filepath, errno, strerror(errno));
		return false;
	}

	if (-1 == stat(filepath, &st)) {
		printf("stat %s error, erron: %d - %s\n", filepath, errno, strerror(errno));
		fclose(fp);
		return false;
	}
	fsize = st.st_size;

	if (!MD5_Init(&md5_ctx)) {
		printf("md5_init error, errno: %d - %s\n", errno, strerror(errno));
		fclose(fp);
		return false;
	}

	if (fsize <= ONCE_MD5SUM_FILE_SIZE) {
		while (true) {
			rlen = fread(buf, 1, CHUNK_SIZE, fp);
			if (rlen == 0) {
				if (feof(fp)) break;
				else {
					printf("fread error, errno: %d - %s\n", errno, strerror(errno));
					fclose(fp);
					return false;
				} 
			}
			
			buf[rlen] = '\0';
			if (!MD5_Update(&md5_ctx, buf, rlen)) {
				printf("md5_update error, errno: %d - %s\n", errno, strerror(errno));
				fclose(fp);
				return false;
			}
		}
	} else {
		chunkNum = fsize / SAMPLE_POS;
		pos[0] = 0;
		pos[SAMPLE_POS - 1] = fsize - CHUNK_SIZE; /* 结尾位置 */
		for(i = 1; i < SAMPLE_POS - 1; i++) {  /* 去掉头尾的采集点 */
			pos[i] = i * chunkNum; /* 可能有重复的采集点，但概率很小，不影响 */
		}
		
		for (i = 0; i < SAMPLE_POS; i++) {
			if (-1 == fseek(fp, pos[i], SEEK_SET)) {
				printf("fseek error, errno: %d - %s\n", errno, strerror(errno));
				fclose(fp);
				return false;
			}
			
			rlen = fread(buf, 1, CHUNK_SIZE, fp);
			if (rlen == 0) {
				if (feof(fp)) break;
				else {
					printf("fread error, errno: %d - %s\n", errno, strerror(errno));
					fclose(fp);
					return false;
				}
			}

			if (!MD5_Update(&md5_ctx ,buf, rlen)) {
				printf("md5_update error, errno: %d - %s\n", errno, strerror(errno));
				fclose(fp);
				return false;
			}
		}
		
	}

	fclose(fp);
	if (!MD5_Final(md5Raw, &md5_ctx)) {
		printf("md5_final error, errno: %d - %s\n", errno, strerror(errno));
		return false;
	}
	_Md5Raw2Hex(md5Raw, md5, len);
	return true;
}

int main(int argc, const char *argv[])
{
	if (argc < 2) {
		printf("Usage: %s filepath\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	char md5Hex[MD5_HEX_BUF_SIZE + 1] = {0};
	if (Md5Sum(argv[1], md5Hex, sizeof(md5Hex))) printf("%s\n", md5Hex);
	else printf("md5Sum error\n");
	
	exit(EXIT_SUCCESS);
}