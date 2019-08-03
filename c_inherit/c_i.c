#include <stdio.h>

struct obj {
    int m_a;
    int m_b;
    void (*say)(void *obj, const char *name);
};

struct son {
    struct obj m_parent;

    int m_a;
    int m_b;

    union {
        int m_c;
        int m_d;
    } m_u;
    void (*say)(void *obj, const char *name);

#define m_c m_u.m_c
#define m_d m_u.m_d
};

void parent_say(void *obj, const char *name)
{
    struct obj* parent = (struct obj*)obj;
    printf("i am parent, name: %s, m_a = %d\n", name, parent->m_a);
}

void son_say(void *obj, const char *name)
{
    struct son* son = (struct son*)obj;
    printf("i am son, name: %s, m_a = %d\n", name, son->m_a);
}

int main()
{
   struct obj p;
   struct son s;
   p.m_a = 1;
   p.say = parent_say;

   s.m_parent = p;
   s.m_a = 2;
   s.m_c = 10;
   s.say = son_say;

   p.say(&p, "p");
   s.say(&s, "s");
   p.say(&s, "p");

   return 0;
}
