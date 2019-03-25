#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define ASCII_MAX 128

int main(int argc, const char *argv[])
{
	int i = 0;
	printf("%10s %10s %10s %10s %10s %10s %10s %10s %10s %10s %10s %10s", "index:", "ascii:", "isalnum:", "isalpha:", 
					"iscntrl:", "isdigit:", "isxdigit:", "isgraph:", "isprint:", "ispunct:", "isspace:", "isupper:");
	printf("\n");
	for (i = 0; i < ASCII_MAX; i++) {
		printf("%10d %10c %10d %10d %10d %10d %10d %10d %10d %10d %10d %10d", i, isprint(i) ? i : ' ', isalnum(i) ? 1 :0,
				isalpha(i) ? 1 :0, iscntrl(i) ? 1 :0, isdigit(i) ? 1 :0, isxdigit(i) ? 1 :0, isgraph(i) ? 1 :0, 
				isprint(i) ? 1 :0, isprint(i) ? 1 :0, ispunct(i) ? 1 :0, isspace(i) ? 1 :0, isupper(i) ? 1 :0);
		printf("\n");
	}
	printf("\n");
	return 0;
}