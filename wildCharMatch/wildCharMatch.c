#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

/* 匹配一个字符的宏 */
#define MATCH_CHAR(c1, c2, ignoreCase)  ((c1 == c2) || ((ignoreCase == true) &&(tolower(c1) == tolower(c2))))

/*	@func: 
 *		通配符匹配
 *	
 *	@param:
 *		src: 源字符串
 *		pattern：含有通配符( * 或 ? 号)的字符串
 *		ignoreCase：是否区分大小写，true:不区分, false:区分
 *
 *	@return:
 *		true: src匹配pattern
 *		false: src不匹配pattern
 *
 *  @warn:
 *		不支持*?的规则
 *		不支持src中含有*或?的字符串
 */
bool WildCharMatch(const char *src, const char *pattern, const bool ignoreCase)
{
	if (!src || !pattern) return false;
	
	while (*src || *pattern) {
		if (*pattern == '*') {	
			while ((*pattern == '*') || (*pattern == '?')) pattern++; /* 以防万一，检查?号 */
			if (!*pattern) return true; /* pattern仅包含*或?, 任何字符串都会匹配正确 */
			
			/* 在src中查找第一个非*或非？字符后一个字符匹配的字符 */
			while (*src && (!MATCH_CHAR(*src, *pattern, ignoreCase))) src++; 
			if (!*src) return false; /* src走到底都找不到一个匹配的字符，则匹配失败 */
			src++; pattern++; /* 跳过匹配的字符 */
			if (!*src && !*pattern) return true; /* src 和 pattern 走到底，匹配成功 */
			else if (!*pattern) return false; /* 仅pattern走到底，还有src字符串没有匹配，匹配失败 */
			else if (!*src) { /* 仅src走到底， pattern剩余的不都是*号， 匹配失败 */
				while ((*pattern == '*')) pattern++;
				if (!*pattern) return true; /* pattern仅包含*, 任何字符串都会匹配正确 */
				else return false;
			} 
			
			return WildCharMatch(src, pattern, ignoreCase);
		} else {
			if (MATCH_CHAR(*src, *pattern, ignoreCase) || ('?' == *pattern)) {
				src++; pattern++;
			} else return false;
			
			if (!*src && !*pattern) return true; 
			else if (!*pattern) return false; /* 仅pattern走到底，还有src字符串没有匹配，匹配失败 */
			else if (!*src) { /* 仅src走到底， pattern剩余的不都是*号， 匹配失败 */
				while ((*pattern == '*')) pattern++;
				if (!*pattern) return true; /* pattern仅包含*, 任何字符串都会匹配正确 */
				else return false;
			} 
		}
	}
	return false;
}


int main(int argc, const char *argv[])
{
	assert(!WildCharMatch("12345", NULL, true));
	assert(!WildCharMatch(NULL, "12345", true));
	assert(WildCharMatch("12345", "12345", true));
	assert(WildCharMatch("12345", "12345", false));
	assert(WildCharMatch("12345ab", "12345AB", true));
	assert(!WildCharMatch("12345ab", "12345AB", false));
	assert(WildCharMatch("12345ab", "12345AB", true));
	assert(WildCharMatch("12345ab", "?2345ab", true));
	assert(!WildCharMatch("12345ab", "?12345ab", true));
	assert(WildCharMatch("12345ab", "123?5ab", true));
	assert(!WildCharMatch("12345ab", "123?45ab", true));
	assert(WildCharMatch("12345ab", "12345a?", true));
	assert(WildCharMatch("12345ab", "*12345ab", true));
	assert(WildCharMatch("12345ab", "123*45ab", true));
	assert(WildCharMatch("12345ab", "123*ab", true));
	assert(WildCharMatch("12345ab", "123*", true));
	assert(WildCharMatch("12345ab", "12*4*b", true));
	assert(WildCharMatch("12345ab", "*12*45*", true));
	assert(WildCharMatch("12345ab", "?23*45ab", true));
	assert(WildCharMatch("12345ab", "1?3*45ab", true));
	assert(WildCharMatch("12345ab", "123*45a?", true));
	assert(WildCharMatch("12345ab", "123?*ab", true));
	assert(!WildCharMatch("12345ab", "12345ab?", true));
	assert(!WildCharMatch("12345ab", "12345ab*?", true));
	printf("Test %s OK\n", argv[0]);
	return 0;
}
