#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define FIEL_HEADER_FORMAT_MAX 0XF
#define FILE_HEADER_FORMAT_END 0X0

/* 文件类型头标识,        最多支持255种标识 */
enum fileHeaderFormat {
	UTF8_BOM = 0X1,
	UCS2_BE_BOM,
	UCS2_LE_BOM,
	ELF64_BIT_LSB_EXE, /* 可执行文件 */
	MICROSOFE_OOXML, /* docx, xlsx */
	CDF_V2_DOC_LE, /* CDF V2 Document, Little Endian, doc */
	ENUM_FILE_HEADER_FORMAT_MAX,
};


const char *fileHeaderFormatStr[] = {
	NULL,
	"UTF8_BOM",
	"UCS2_BE_BOM",
	"UCS2_LE_BOM",
	"ELF64_BIT_LSB_EXE",
	"MICROSOFE_OOXML",
	"CDF_V2_DOC_LE",
	"UNKNOW_FILE_HEADER_FORMAT",
	
};

/* 文件头标识定义，从长的头标识到短的进行定义，避免头标识前缀相同造成误判；
 * 当然 短头标识+正文内容 = 长头标识 的情况也是存在的，也会造成误判
 */
unsigned char formatType[][FIEL_HEADER_FORMAT_MAX + 2] = {
	{0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1, FILE_HEADER_FORMAT_END, (unsigned char)CDF_V2_DOC_LE},
	{0X7f, 0X45, 0X4c, 0X46, 0X2, 0X1, 0X1, FILE_HEADER_FORMAT_END, (unsigned char)ELF64_BIT_LSB_EXE},
	{0X50, 0X4b, 0x3, 0x4, 0xa, FILE_HEADER_FORMAT_END, (unsigned char)MICROSOFE_OOXML},
	{0XEF, 0XBB, 0XBF, FILE_HEADER_FORMAT_END, (unsigned char)UTF8_BOM},
	{0XFE, 0XFF, FILE_HEADER_FORMAT_END, (unsigned char)UCS2_BE_BOM},
	{0XFF, 0XFE, FILE_HEADER_FORMAT_END, (unsigned char)UCS2_LE_BOM},	
};


/* @func:
 *		解析文件头字节，判断是什么文件
 *
 * @param:
 *		文件路径
 *
 * @return:
 *		成功：文件特征标识
 *		失败：
 *			-1：文件操作失败
 *			ENUM_FILE_HEADER_FORMAT_MAX: 无法识别文件特征
 */
unsigned char ParseFileFormat(const char *filepath)
{
	unsigned char format = 0;
	int i = 0, j = 0;
	long len = 0;
	unsigned char buf[FIEL_HEADER_FORMAT_MAX + 1] = {0};
	FILE *fp = NULL;
	
	if (!(fp = fopen(filepath, "rb"))) {
		printf("fopen %s error, errno: %d - %s\n", filepath, errno, strerror(errno));
		return -1;
	}
	
	if (-1 == fseek(fp, 0L, SEEK_END)) {
		printf("fseek error, errno: %d - %s\n", errno, strerror(errno));
		return -1;
	}
	
	if (-1 == (len = ftell(fp))) {
		printf("ftell error, errno: %d - %s\n", errno, strerror(errno));
		return -1;
	}

	len = len > FIEL_HEADER_FORMAT_MAX ? FIEL_HEADER_FORMAT_MAX : len;
	rewind(fp);
	
	if (len != fread(buf, sizeof(unsigned char), len, fp)) {
		printf("fread error, errno: %d - %s\n", errno, strerror(errno));
		return -1;
	}

	for (i = 0; i < sizeof(formatType) / sizeof(formatType[0]); i++) {
		for (j = 0; j <= FIEL_HEADER_FORMAT_MAX &&  
				formatType[i][j] != FILE_HEADER_FORMAT_END && buf[j] != 0; j++) {
			if (formatType[i][j] != buf[j]) break;
		}
		if (FILE_HEADER_FORMAT_END == formatType[i][j]) {
			fclose(fp);
			return formatType[i][j + 1]; /* 找到符合的文件头编码，返回标识 */
		}
	}

	fclose(fp);
	return (unsigned char)ENUM_FILE_HEADER_FORMAT_MAX;
}

void PrintFileHeaderFormat(unsigned char format)
{
	size_t elem = sizeof(fileHeaderFormatStr) / sizeof(fileHeaderFormatStr[0]);
	if (format >= elem) {
		printf("format invalid\n");
		return ;
	}
	printf("format: %s\n", fileHeaderFormatStr[format]);
}


int main(int argc, const char *argv[])
{
	if (argc < 2) {
		printf("Usage: %s filename\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	unsigned char format = 0;
	format = ParseFileFormat(argv[1]);
	PrintFileHeaderFormat(format);

	return 0;
}
