#include <stdio.h>
#include <regex.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#define ERRBUF_SIZE 1024

int main(int argc, const char *argv[])
{
	regex_t reg;
	int ret = 0, i = 0;
	int cflags = REG_EXTENDED | REG_ICASE;
	int eflags = 0;
	size_t nmatch = 1;
	regmatch_t pmatch[1];
	const char *regex = "[0-9]+.[0-9]+.[0-9]+.[0-9]+:[0-9]+";
	const char *str = "192.3.4.3:342world 192.46.45.12:34 12.23.34.45 apple 123.43.45.34.23:3hello";
	const char *ptr = str;
	char errbuf[ERRBUF_SIZE] = {0};
	
	if ((ret = regcomp(&reg, regex, cflags))) goto err;
	
	while (true) {
		ptr += i;
		printf("\nstr: %s\n", ptr);
		if ((ret = regexec(&reg, ptr, nmatch, pmatch, eflags))) goto err;
		
		printf("rm_so - rm_eo: %d - %d\n", pmatch[0].rm_so, pmatch[0].rm_eo);
		if (pmatch[0].rm_so >= 0 && pmatch[0].rm_eo > pmatch[0].rm_so) {
			printf("sub str: %.*s\n", pmatch[0].rm_eo - pmatch[0].rm_so, ptr + pmatch[0].rm_so);
		}
		if (-1 == (i = pmatch[0].rm_eo) || -1 == pmatch[0].rm_so) break;
	}
	goto out;

err:
	regerror(ret, &reg, errbuf, sizeof(errbuf));
	printf("errbuf: %s\n", errbuf);
	
out:
	regfree(&reg);
	return 0;
}
