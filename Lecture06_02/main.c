#include <stdio.h>

typedef struct
{
    int m;
    int arr[2];
} SType;

int main()
{
    SType a = { 1, {2, 3} };
    SType b = a;
    SType c;
    c = a;
    a.m = 7;
    a.arr[0] = 8;
    
    printf("a: {%d, %d, %d}\r\n", a.m, a.arr[0], a.arr[1]);
    printf("b: {%d, %d, %d}\r\n", b.m, b.arr[0], b.arr[1]);
    printf("c: {%d, %d, %d}", c.m, c.arr[0], c.arr[1]);
}
