package main

import "unsafe"

/*
#include <stdio.h>
#include <stdlib.h>

void myprint(const char *str)
{
    printf("%s\n", str);
}
*/
import "C"

func main() {
    name := "I am hello"
    c_name := C.CString(name)
    defer C.free(unsafe.Pointer(c_name))
    C.myprint(c_name)
}
