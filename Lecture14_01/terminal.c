/* 제공 코드: getchar는 문자만 반환하므로 키 해제는 Windows로 보완한다.
 * 입력 스레드는 게임 상태를 건드리지 않고 원자적 비트 메시지만 전달한다. */
#include <windows.h> // 콘솔 모드·키 상태·시간 API이다.
#include <process.h> // C 런타임과 함께 사용하는 _beginthreadex이다.
#include <stdio.h> // 실제 문자 입력 getchar와 ANSI 출력이다.
#include "terminal.h" // 외부에 공개할 함수이다.
enum { GAS = 1, LEFT = 2, RIGHT = 4, RESTART = 8, QUIT = 16 }; // 전달할 입력 비트이다.
static HANDLE inputHandle, outputHandle, reader; // 콘솔과 입력 스레드 핸들이다.
static DWORD oldInput, oldOutput; // 실행 전 콘솔 모드를 보관한다.
static volatile LONG pending, stopping; // Interlocked 함수로만 접근하는 공유 값이다.
static LARGE_INTEGER frequency, previous; // 고해상도 시간 측정이다.
static int opened, held; // 복원 필요 여부와 현재 누른 키이다.
static HWND focus; // 입력을 받은 창을 기억하여 포커스 이탈 시 해제한다.

static unsigned __stdcall ReadCharacters(void *unused) // getchar 전용 스레드이다.
{ // 입력 스레드 시작.
    (void)unused; // 스레드 인자는 사용하지 않는다.
    while (!InterlockedCompareExchange(&stopping, 0, 0)) // 종료 요청 전까지 반복한다.
    { // 문자 입력 시작.
        int ch = getchar(); // Enter 없이 실제 문자를 받는다. 기다려도 게임 루프는 계속된다.
        LONG bit = 0; // 이번 문자의 명령이다.
        if (ch == EOF) { InterlockedOr(&pending, QUIT); break; } // 입력 종료를 전달한다.
        if (ch == 'w' || ch == 'W') bit = GAS; // 가속 시작이다.
        if (ch == 'a' || ch == 'A') bit = LEFT; // 왼쪽 이동 시작이다.
        if (ch == 'd' || ch == 'D') bit = RIGHT; // 오른쪽 이동 시작이다.
        if (ch == 'r' || ch == 'R') bit = RESTART; // 시작 또는 재시작이다.
        if (ch == 'q' || ch == 'Q') bit = QUIT; // 정상 종료이다.
        InterlockedOr(&pending, bit); // 게임 스레드에 명령을 안전하게 전달한다.
    } // 입력 반복 끝.
    return 0; // 스레드를 종료한다.
} // 입력 함수 끝.

int OpenTerminal(void) // 터미널 준비에 실패하면 실행하지 않는다.
{ // 준비 시작.
    inputHandle = GetStdHandle(STD_INPUT_HANDLE); // 표준 입력 핸들이다.
    outputHandle = GetStdHandle(STD_OUTPUT_HANDLE); // 표준 출력 핸들이다.
    if (!GetConsoleMode(inputHandle, &oldInput) || !GetConsoleMode(outputHandle, &oldOutput)) return 0; // 실제 콘솔이 필요하다.
    DWORD mode = (oldInput | ENABLE_EXTENDED_FLAGS) & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_QUICK_EDIT_MODE | ENABLE_PROCESSED_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT); // 줄 대기와 에코·선택 정지를 끈다.
    if (!SetConsoleMode(inputHandle, mode)) return 0; // 입력 설정 실패이다.
    if (!SetConsoleMode(outputHandle, oldOutput | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) // ANSI ESC를 해석하게 한다.
    { // 출력 설정 실패 시 복원한다.
        SetConsoleMode(inputHandle, oldInput); // 입력 설정을 되돌린다.
        return 0; // 실행 실패이다.
    } // 출력 설정 확인 끝.
    opened = 1; // 이후 종료 함수에서 모드를 복원한다.
    setvbuf(stdin, NULL, _IONBF, 0); // getchar가 런타임 내부에 여러 문자를 쌓지 않게 한다.
    QueryPerformanceFrequency(&frequency); // 초당 카운터 수이다.
    QueryPerformanceCounter(&previous); // 첫 프레임의 기준 시각이다.
    reader = (HANDLE)_beginthreadex(NULL, 0, ReadCharacters, NULL, 0, NULL); // 입력만 별도로 기다린다.
    if (reader == NULL) { CloseTerminal(); return 0; } // 스레드 생성 실패도 복원한다.
    fputs("\x1b[?1049h\x1b[2J\x1b[H\x1b[?25l", stdout); // 별도 화면을 열고 커서를 숨긴다.
    return 1; // 준비 성공이다.
} // 준비 끝.

void CloseTerminal(void) // Q로 종료할 때 원래 터미널로 돌아간다.
{ // 종료 준비 시작.
    if (!opened) return; // 준비하지 않은 콘솔은 건드리지 않는다.
    InterlockedExchange(&stopping, 1); // 입력 반복에 종료를 요청한다.
    if (reader != NULL) // 입력 대기를 깨워 정상적으로 합류한다.
    { // 스레드 종료 시작.
        INPUT_RECORD wake = {0}; // getchar를 깨울 입력 레코드이다.
        DWORD written; // 기록된 레코드 수이다.
        wake.EventType = KEY_EVENT; // 키 이벤트이다.
        wake.Event.KeyEvent.bKeyDown = TRUE; // 문자 하나를 제공한다.
        wake.Event.KeyEvent.wRepeatCount = 1; // 한 번만 입력한다.
        wake.Event.KeyEvent.uChar.UnicodeChar = L'\n'; // 종료용 줄바꿈이다.
        WriteConsoleInputW(inputHandle, &wake, 1, &written); // 현재 또는 곧 시작할 입력 대기를 깨운다.
        CancelSynchronousIo(reader); // 이미 대기 중인 읽기도 취소한다.
        WaitForSingleObject(reader, INFINITE); // 입력 스레드 종료 뒤 모드를 복원한다.
        CloseHandle(reader); // 스레드 핸들을 반납한다.
    } // 스레드 정리 끝.
    fputs("\x1b[0m\x1b[?25h\x1b[?1049l", stdout); // 색·커서·원래 화면을 복원한다.
    fflush(stdout); // 복원 문자를 실제로 전달한다.
    SetConsoleMode(inputHandle, oldInput); // 원래 입력 설정이다.
    SetConsoleMode(outputHandle, oldOutput); // 원래 출력 설정이다.
    opened = 0; // 이중 복원을 막는다.
} // 종료 준비 끝.

Input ReadInput(void) // 키 반복 속도에 게임 속도가 좌우되지 않도록 상태로 바꾼다.
{ // 입력 수집 시작.
    LONG keys = InterlockedExchange(&pending, 0); // 새 문자 명령을 한 번만 가져온다.
    Input input = {0}; // 기본값은 아무 조작도 없는 상태이다.
    if (keys != 0) focus = GetForegroundWindow(); // 마지막 문자 입력 창이다.
    held |= keys & (GAS | LEFT | RIGHT); // getchar가 받은 키만 눌림 상태로 등록한다.
    if (!(GetAsyncKeyState('W') & 0x8000)) held &= ~GAS; // W를 놓은 순간 가속을 끈다.
    if (!(GetAsyncKeyState('A') & 0x8000)) held &= ~LEFT; // 왼쪽 키 해제를 확인한다.
    if (!(GetAsyncKeyState('D') & 0x8000)) held &= ~RIGHT; // 오른쪽 키 해제를 확인한다.
    if (GetForegroundWindow() != focus) held = 0; // 다른 창을 조작하면 가속하지 않는다.
    input.accelerate = (held & GAS) != 0; // 가속 상태이다.
    input.steer = ((held & RIGHT) != 0) - ((held & LEFT) != 0); // 양쪽 동시 누름은 중립이다.
    input.restart = (keys & RESTART) != 0; // R 문자 명령이다.
    input.quit = (keys & QUIT) != 0; // Q 문자 명령이다.
    return input; // 이번 프레임에 사용할 입력이다.
} // 입력 수집 끝.

float FrameTime(void) // 프레임 속도와 주행 속도를 분리한다.
{ // 시간 측정 시작.
    LARGE_INTEGER now; // 현재 카운터이다.
    QueryPerformanceCounter(&now); // 실제 시각을 읽는다.
    float dt = (float)((double)(now.QuadPart - previous.QuadPart) / (double)frequency.QuadPart); // 초로 환산한다.
    previous = now; // 다음 프레임의 기준이다.
    return dt; // 경과 시간을 반환한다.
} // 시간 측정 끝.
void RestFrame(void) { Sleep(16); } // 콘솔을 초당 약 수십 회 갱신하며 CPU를 쉬게 한다.

int TerminalFits(void) // 창이 작아지면 주행을 잠시 멈춘다.
{ // 크기 검사 시작.
    CONSOLE_SCREEN_BUFFER_INFO info; // 현재 보이는 콘솔 영역이다.
    if (!GetConsoleScreenBufferInfo(outputHandle, &info)) return 0; // 읽기 실패도 정지한다.
    return info.srWindow.Right - info.srWindow.Left + 1 >= 94 && info.srWindow.Bottom - info.srWindow.Top + 1 >= 27; // 계기판과 도로가 들어갈 공간이다.
} // 크기 검사 끝.
