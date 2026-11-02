#include "logger.h"

const char* get_level_str(LogLevel level) {
}

int main(void) {
   


    logger_write(LOG_LEVEL_INFO, __FILE__, __LINE__, "이경빈이 왜 열심히 듣지?");

    return 0;
}