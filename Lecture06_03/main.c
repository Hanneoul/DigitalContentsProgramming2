#include <stdio.h>

typedef struct
{
    int m;
    char* str1;
    char* str2;
} SType;

int main()
{
    SType s;
    SType *pS = &s;

    pS->m = 2021;
    pS->str1 = "난생처음";
    (*pS).str2 = "C 프로그래밍";

    printf("%d %s %s", s.m, s.str1, s.str2);
}
