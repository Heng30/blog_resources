#include <stdio.h>
#include <stddef.h>

/* 强制转换宏 */
#define TYPE_CAST(type, variable) ((type)(variable))

/* 通过结构体成员地址，找到对应的结构体起始地址的宏 */
#define STRUCT_OF(member_addr, type, member) ((type*)((void*)(member_addr) - (offsetof(type, member))))

#if 1
struct a {
    short a;
    short b;
} aa = {0x1, 0xf};

struct b {
    int a;
};

struct c {
    char a[4];
};

int main()
{
    int i = 0;
    struct b *bp = TYPE_CAST(struct b*, &aa);
    struct c *cp = TYPE_CAST(struct c*, &aa);
    struct a *ap = STRUCT_OF(&aa.b, struct a, b);
    
    printf("%d\n", bp->a);
    for (i = 0; i < 4; i++) {
        printf("%#x\t", cp->a[i]);
    }
    printf("\n");

    printf("a: %#x, b: %#x\n", ap->a, ap->b);
    return 0;
}
#endif
