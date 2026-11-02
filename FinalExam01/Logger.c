#include "logger.h"
#include <stdio.h>
#include <time.h>



// [충돌 방지 포인트] 내부 전용 헬퍼 함수도 static 처리
const char* get_level_str(LogLevel level) {
    switch (level) {
    case LOG_LEVEL_INFO:  return "INFO";
    case LOG_LEVEL_WARN:  return "WARN";
    case LOG_LEVEL_ERROR: return "ERROR";
    default:              return "UNKNOWN";
    }
}


void logger_write(LogLevel level, const char* file, int line, const char* message) {

    // 간단한 타임스탬프 출력 예시
    time_t now = time(NULL);
    struct tm* t = localtime(&now);

    printf("[%02d:%02d:%02d] [%s] (%s:%d) - %s\n",
        t->tm_hour, t->tm_min, t->tm_sec,
        get_level_str(level),
        file, line,
        message);
}