#include <stdio.h>
#include "logger.h"

// 로그 번호 카운터 전역 변수의 실물(메모리 할당)
int g_logCounter = 0;

// 로그 출력 함수 정의
void printDebugLog(const char* message)
{
#ifdef _HAN_LOGGER_
    g_logCounter++;
    // 시안(Cyan) 색상으로 포맷에 맞춰 출력 (\033[36m)
    printf("\033[36mdebug log #%d : \"%s\"\033[0m\n", g_logCounter, message);
#else
    // _HAN_LOGGER_가 꺼져있으면 아무 동작도 하지 않음
#endif
}