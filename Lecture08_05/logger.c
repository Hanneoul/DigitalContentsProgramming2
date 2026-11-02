#include <stdio.h>
#include "logger.h"



// 2. 로그를 출력하는 함수 정의
// 매개변수로 전달받은 문자열을 시안 색상과 로그 번호와 함께 출력함
void printDebugLog(const char* message)
{
#ifdef _HAN_LOGGER_

    static int g_logCounter;
    // 로그 번호 카운터 1 증가
    g_logCounter++;

    // ANSI escape code: \033[36m (Cyan 색상 시작), \033[0m (색상 초기화)
    // 로그번호와 입력받은 문자열을 포맷에 맞춰 출력함
    printf("\033[36mdebug log #%d : \"%s\"\033[0m\n", g_logCounter, message);
#else
    // _HAN_LOGGER_ 가 정의되어 있지 않다면 이 함수는 아무 일도 하지 않음 (빈 칸으로 처리됨)
    // 컴파일 단계에서 아예 코드가 제거되거나 무시되므로 성능 저하가 없음
#endif
}