#ifndef _FUNC_LIST_H_
#define _FUNC_LIST_H_

#include <stdbool.h>

/* @func:
 *  销毁管理器
 */
void func_list_destroy(void);

/* @func:
 *  设置函数列表
 */
bool func_list_add(const char *name, void* cb);

/* @func:
 *  获取跳转表指针
 */
void* func_list_get(const char *name);

/* @func:
 *  打印信息
 */
void func_list_dump(void);

#endif
