/* Windows 콘솔 제공 코드의 인터페이스. 게임 규칙에서는 사용하지 않는다. */
#ifndef ROAD_TERMINAL_H // 중복 선언을 막는다.
#define ROAD_TERMINAL_H // 헤더 표시이다.
typedef struct { int accelerate, steer, restart, quit; } Input; // 이번 프레임의 입력이다.
int OpenTerminal(void); // ANSI 출력과 getchar 입력 스레드를 준비한다.
void CloseTerminal(void); // 입력 모드·색·커서를 복원한다.
Input ReadInput(void); // getchar 문자와 키 해제 상태를 합친다.
float FrameTime(void); // 실제 경과 시간을 초 단위로 얻는다.
void RestFrame(void); // CPU를 독점하지 않도록 잠깐 쉰다.
int TerminalFits(void); // 화면이 잘리지 않을 크기인지 검사한다.
#endif // 인터페이스 끝.
