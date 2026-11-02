#ifndef LOGGER_H
#define LOGGER_H

// 로그 레벨 정의
typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;


// 공용 로그 출력 함수 (함수명 충돌 방지를 위해 prefix로 logger_ 사용)
void logger_write(LogLevel level, const char* file, int line, const char* message);

//// 편의를 위한 매크로 (어디서 호출했는지 파일명과 줄 번호 자동 기록)
//#define LOG_INFO(msg)    logger_write(LOG_LEVEL_INFO, __FILE__, __LINE__, msg)
//#define LOG_WARN(msg)    logger_write(LOG_LEVEL_WARN, __FILE__, __LINE__, msg)
//#define LOG_ERROR(msg)   logger_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, msg)

#endif // LOGGER_H