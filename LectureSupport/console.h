/* 강의용 화면 도구: 게임 규칙 없이 입력 한 줄과 ASCII 패널만 담당한다.
 * main.c가 교재이며 이 파일은 제공 코드이다. ANSI 제어문자 없이 출력하여
 * Visual Studio 콘솔과 파일로 저장한 실행 기록에서 같은 배치를 사용한다. */
#ifndef LECTURE_CONSOLE_H // 여러 번 포함해도 정의는 한 번만 읽는다.
#define LECTURE_CONSOLE_H // 포함 여부를 표시한다.
#include <stdio.h> // puts, printf, getchar, EOF의 선언이다.

static inline void panel(const char *title) // 제목을 받는 공통 상태창이다.
{ // 상태창 시작.
    puts("\n+==========================================================+"); // 위 테두리.
    printf("| %-56s |\n", title); // 제목을 왼쪽 정렬한다.
    puts("+==========================================================+"); // 아래 테두리.
} // 상태창 끝.

static inline void track(int x, int active) // 한 객체의 12칸 이동 경로이다.
{ // 경로 출력 시작.
    fputs("  START |", stdout); // 출발 지점을 표시한다.
    for (int cell = 0; cell < 12; ++cell) // 0~11번 칸을 순서대로 방문한다.
        printf(" %c ", active && cell == x ? '@' : '.'); // 활성 객체만 그린다.
    puts("| GOAL"); // 도착 지점을 표시한다.
} // 경로 출력 끝.

static inline int command(void) // 첫 문자 명령을 받고 나머지 한 줄은 소비한다.
{ // 입력 시작: Enter도 한 번의 명령이다.
    int key = getchar(); // EOF를 보관할 수 있도록 int를 쓴다.
    int rest = key; // 이미 읽은 문자를 기준으로 남은 줄을 검사한다.
    while (rest != '\n' && rest != EOF) // 줄 끝 또는 입력 끝까지 버린다.
        rest = getchar(); // 다음 문자를 소비한다.
    return key; // 첫 문자 또는 EOF를 호출자에게 돌려준다.
} // 입력 끝.
#endif // 중복 포함 방지 끝.
