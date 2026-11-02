/*
================================================================================
 [모듈 사용 설명서] Logger Module (logger.h)
================================================================================

 1. 모듈 개요
 - 프로그램 전체에서 공용으로 사용하는 디버그 로그 출력 및 카운팅 시스템임.
 - ANSI Escape Code를 활용하여 터미널 창에 시안(Cyan) 색상으로 로그를 예쁘게 출력함.
 - 조건부 컴파일(#define)을 지원하여 릴리즈(Release) 시 로그 출력을 완전히 끌 수 있음.

 2. 제공하는 기능 (API 목록)
 - void printDebugLog(const char *message)
   : 전달받은 문자열을 로그 번호와 함께 시안 색상으로 출력함.
 - extern int g_logCounter
   : 지금까지 출력된 총 로그의 개수를 담고 있는 공유 전역 변수.

 3. 모듈 사용 방법 (How to Use)
 ① 이 헤더 파일("logger.h")을 사용할 소스 파일 상단에 include 함.
    #include "logger.h"
 ② 필요할 때마다 printDebugLog("출력할 내용") 함수를 호출함.
 ③ 로그 기능을 끄고 싶다면 이 파일 상단의 #define _HAN_LOGGER_ 를 주석 처리함.

 4. 사용 예제 코드 (Example)
    #include <stdio.h>
    #include "logger.h"

    #define _HAN_LOGGER_

    int main(void) {
        printDebugLog("시스템 초기화 중...");
        printDebugLog("플레이어 데이터 로드 완료");

        printf("총 로그 발생 횟수: %d\n", g_logCounter);
        return 0;
    }
================================================================================
*/

#ifndef LOGGER_H
#define LOGGER_H

// 디버그 로그 스위치 (이 줄을 주석 처리하면 로거 기능이 완전히 꺼짐)
#define _HAN_LOGGER_

// [전역 변수 공유 선언]
// 진짜 실물은 logger.c에 존재하며, 이 모듈을 쓰는 모든 파일이 이 변수를 공유함.
extern int g_logCounter;

// [함수 선언]
// 문자열을 받아 시안 색상으로 로그를 찍어주는 메인 함수
void printDebugLog(const char* message);

#endif
