#ifndef LOGGER_H
#define LOGGER_H

// 디버그 로그 스위치 (이 줄을 주석 처리하면 로거 기능이 완전히 꺼짐)
#define _HAN_LOGGER_

// [함수 선언]
// 문자열을 받아 시안 색상으로 로그를 찍어주는 메인 함수
void printDebugLog(const char* message);

#endif