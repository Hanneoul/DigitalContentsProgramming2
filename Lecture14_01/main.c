/*
==============================================================================
 [강의 자료] Lecture14_01 | NEON HIGHWAY : 엔진도 쉬어야 달린다
==============================================================================

 1. 오늘 만들 게임
    파란 차를 운전하여 빨간 장애물을 피하는 고전 아케이드 레이싱이다.
    W를 누르면 파란 가속 게이지가 차고 도로가 빨라진다. 놓으면 관성으로 감속한다.
    계속 밟으면 엔진 열이 초록에서 빨강으로 바뀐다. 가득 차면 BOOM!!!
    Fuel Point에 늦으면 연료가 바닥난다. 멈춰 있어도 시간은 기다려 주지 않는다.
    부스트는 없다. 빨리 달리기와 쉬어 가기의 균형이 생존 기술이다.
    숫자는 달린 거리뿐이다. 속도·열·연료·남은 시간은 숫자로 표시하지 않는다.

 2. 앞 강의가 실제 게임으로 연결되는 지도
       getchar 입력 ---> Input ---> UpdateGame ---> Render
                            ^           |               |
                            +-----------+---------------+
    Game 구조체: 게임 전체 상태. Obstacle 배열: 재사용하는 Object Pool.
    StartGame: 초기화, UpdateGame: 매 프레임 갱신, active=0: 객체 반납.
    onEvent 콜백: REFUEL / HIT / BOOM / DRY 사건을 화면에 전달한다.
    game.c는 콘솔을 모르고, render.c는 게임 규칙을 바꾸지 않는다.

 3. getchar와 키를 놓는 순간은 다른 정보이다
    getchar는 문자(character)를 하나 가져오는(get) 표준 C 함수이다.
    하지만 키 해제는 문자가 아니다. 표준 getchar만으로는 해제를 알 수 없다.
    terminal.c는 줄 입력을 꺼서 Enter 없이 getchar로 W/A/D/R/Q를 받는다.
    Windows GetAsyncKeyState의 현재 눌림 상태로 놓은 키를 해제한다.
    getchar는 기다리는 함수이므로 제공된 입력 스레드에서 실행한다.
    Thread(실)는 실행 흐름 한 가닥이다. 이 수업에서는 제공 코드로 사용한다.
    입력 스레드는 게임을 수정하지 않고 Interlocked로 명령만 전달한다.
    이 때문에 입력이 없어도 연료와 도로는 계속 갱신된다.

 4. ANSI ESC Seq로 블록 그림 만들기
    Sequence는 순서 있는 명령 묶음이다. ESC(escape)는 일반 글자가 아닌
    제어 명령의 시작을 알린다. \x1b[H는 커서 원점, \x1b[48;5;33m은 배경색이다.
    같은 색의 공백 두 칸을 한 픽셀처럼 사용한다. 숫자 색 코드는 화면에 안 보인다.
    매번 화면 전체를 지우지 않고 원점으로 돌아가 덮어써 깜빡임을 줄인다.
    Q로 끝내면 원래 색·커서·콘솔 입력 모드를 복원한다.

 5. 수업에서는 한 덩어리씩 읽는다
    먼저 main의 루프 → game.c의 가속/냉각/연료 → 장애물 → 보급선 순서이다.
    dt(delta time)는 지난 프레임부터 흐른 초이다. 값 변화량에 dt를 곱한다.
    연료는 제한 시간 게이지이다. 청록색 보급선 통과 시 가득 채워진다.
    보급으로 엔진까지 식지는 않는다. 과열·연료 소진은 보급보다 먼저 검사한다.
    장애물은 이동 거리에 따라 생성되어 정차 중 쌓이지 않는다.
    실제 화면·Windows 처리는 제공 코드이므로 처음부터 모두 타이핑하지 않는다.

 6. 조작과 실험
    R: 시작/게임 오버 후 재도전. W 길게: 가속. A/D: 차선 이동. Q: 종료.
    W를 놓으면 파란 게이지가 줄고 엔진이 식는지 확인한다.
    계속 밟으면 빨간 경고 뒤 폭발, 아무것도 안 누르면 연료 소진이다.
    냉각을 섞어 달리면 청록 보급선에서 연료가 다시 찬다.
    화면은 가로 94칸·세로 27줄 이상인 Windows 콘솔/Terminal을 권장한다.
    영문 입력 상태로 플레이한다. 키 입력과 게임 속도는 키 반복 설정과 무관하다.
==============================================================================
*/
#include <stdio.h> // 표준 출력과 버퍼 설정이다.
#include <stdlib.h> // atexit로 정상 종료 시 콘솔을 복원한다.
#include <time.h> // 새 게임의 장애물 배치 시작값이다.
#include "game.h" // 상태와 게임 규칙이다.
#include "terminal.h" // getchar 입력과 Windows 제공 코드이다.
#include "render.h" // ANSI 블록 화면이다.

int main(void) // 강의에서는 이 짧은 연결 코드부터 읽는다.
{ // 프로그램 시작.
    static char outputBuffer[65536]; // 한 프레임의 출력을 모으는 버퍼이다.
    Game game; // 이번 게임의 상태이다.
    setvbuf(stdout, outputBuffer, _IOFBF, sizeof outputBuffer); // 조각 출력의 깜빡임을 줄인다.
    if (!OpenTerminal()) // 실제 Windows 콘솔과 ANSI 출력이 필요하다.
    { // 실행 환경 오류이다.
        fputs("Open this game in a Windows console with ANSI support.\n", stderr); // 해결 안내이다.
        return 1; // 실패 종료이다.
    } // 환경 검사 끝.
    atexit(CloseTerminal); // 정상 종료 시 커서와 콘솔 설정을 복원한다.
    StartGame(&game, (unsigned int)time(NULL), OnGameEvent); // 콜백과 상태를 준비한다.
    game.phase = READY; // R을 누르기 전에는 연료가 줄지 않는다.
    for (;;) // 실시간 게임 루프이다.
    { // 프레임 시작.
        Input input = ReadInput(); // Input: 실제 getchar 명령과 키 해제 상태이다.
        float dt = FrameTime(); // 이번 프레임의 실제 경과 시간이다.
        if (input.quit) break; // Q를 누르면 원래 콘솔로 돌아간다.
        if (!TerminalFits()) // 작은 창에서는 계기판이 줄바꿈되므로 주행을 잠시 멈춘다.
        { // 크기 안내 시작.
            fputs("\x1b[H\x1b[2JPlease enlarge the console window.\nROAD PAUSED  |  Q: quit", stdout); // 숫자 없이 확대를 요청한다.
            fflush(stdout); // 안내를 즉시 보여 준다.
            RestFrame(); // 대기 중에도 CPU를 쉬게 한다.
            continue; // 연료와 엔진 상태를 그대로 보존한다.
        } // 크기 안내 끝.
        if (input.restart && game.phase != RUNNING) StartGame(&game, (unsigned int)time(NULL), OnGameEvent); // 실행 중 리셋으로 실패를 피할 수 없다.
        UpdateGame(&game, dt, input.accelerate, input.steer); // Update: 게임 규칙을 한 번 진행한다.
        Render(&game); // Render: 상태를 색 블록과 게이지로 그린다.
        RestFrame(); // 남는 시간에는 CPU를 쉬게 한다.
    } // 게임 루프 끝.
    return 0; // atexit가 콘솔 복원을 실행한다.
} // 프로그램 끝.
