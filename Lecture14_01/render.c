/* 배경색을 가진 공백 두 칸을 한 픽셀처럼 사용한다. 이미지 파일은 필요 없다. */
#include <stdio.h> // ANSI ESC 문자열 출력이다.
#include "render.h" // 화면 함수와 상태 선언이다.
enum { WIDTH = 30, HEIGHT = 20 }; // 콘솔 블록 화면 크기이다.
enum { GRASS = 22, LEAF = 28, ROAD = 236, LINE = 250, RED = 196, WHITE = 231, BLUE = 33, GLASS = 159, TIRE = 232, GOLD = 220 }; // ANSI 팔레트이다.
static int pixels[HEIGHT][WIDTH]; // 이번 프레임의 블록 색을 보관한다.
static const char *notice = "KEEP YOUR ENGINE COOL"; // 마지막 사건의 문구이다.

void OnGameEvent(Event event) // 12~13강의 콜백을 화면에 적용한다.
{ // 사건 반응 시작.
    if (event == REFUEL) notice = "FUEL POINT! TANK FULL"; // 보급 성공이다.
    if (event == HIT) notice = "CRASH! GAME OVER"; // 차량 충돌이다.
    if (event == BOOM) notice = "BOOM!!! GAME OVER"; // 엔진 폭발이다.
    if (event == DRY) notice = "OUT OF FUEL! GAME OVER"; // 연료 소진이다.
} // 사건 반응 끝.

static void Pixel(int x, int y, int color) // 화면 밖으로 나간 픽셀은 버린다.
{ // 픽셀 기록 시작.
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) pixels[y][x] = color; // 유효한 칸만 기록한다.
} // 픽셀 기록 끝.

static void Car(int lane, int y, int color) // 작은 블록 자동차를 그린다.
{ // 자동차 그리기 시작.
    int x = 8 + lane * 7; // 차선 중심을 화면 가로 위치로 바꾼다.
    for (int dy = -1; dy <= 1; ++dy) // 차체 세 줄이다.
        for (int dx = -1; dx <= 1; ++dx) Pixel(x + dx, y + dy, color); // 차체를 채운다.
    Pixel(x, y - 1, GLASS); // 앞 유리이다.
    Pixel(x - 1, y + 1, TIRE); // 왼쪽 타이어이다.
    Pixel(x + 1, y + 1, TIRE); // 오른쪽 타이어이다.
} // 자동차 그리기 끝.

static void Gauge(float value, int color) // 숫자와 눈금 없이 색 블록으로만 표시한다.
{ // 게이지 시작.
    int fill = (int)(value * 20 + 0.5f); // 색을 채울 칸 수이다.
    fputs("  ", stdout); // 도로와 간격을 둔다.
    for (int i = 0; i < 20; ++i) // 게이지 폭만큼 반복한다.
        printf("\x1b[48;5;%dm ", i < fill ? color : 238); // 빈 영역은 짙은 회색이다.
    fputs("\x1b[0m\x1b[48;5;234m", stdout); // HUD 배경으로 복귀한다.
} // 게이지 끝.

static void Dashboard(const Game *game, int row) // 도로 오른쪽의 계기판이다.
{ // 계기판 한 줄 시작.
    float remaining = game->nextFuel - game->distance; // 보급선이 가까운지 판단한다.
    switch (row) // 해당 줄에 필요한 정보만 출력한다.
    { // 줄 선택 시작.
    case 0: printf("  \x1b[97mDISTANCE  %.0f m", game->distance); break; // 유일한 숫자 점수이다.
    case 2: fputs("  \x1b[94mACCEL", stdout); break; // 파랑 가속 게이지 이름이다.
    case 3: Gauge(game->speed, BLUE); break; // 키를 놓으면 서서히 줄어든다.
    case 5: fputs(game->heat >= 0.8f ? "  \x1b[91mENGINE HEAT  / DANGER!" : "  \x1b[92mENGINE HEAT", stdout); break; // 과열 경고이다.
    case 6: Gauge(game->heat, game->heat >= 0.8f ? RED : 46); break; // 거의 차면 빨강이다.
    case 8: fputs("  \x1b[93mFUEL", stdout); break; // 남은 시간의 시각적 표현이다.
    case 9: Gauge(game->fuel, game->fuel < 0.22f ? RED : GOLD); break; // 시간 숫자는 표시하지 않는다.
    case 11: fputs(remaining <= 100 ? "  \x1b[96mFUEL POINT AHEAD" : "  \x1b[37mCHASE THE NEXT FUEL POINT", stdout); break; // 다음 목표이다.
    case 13: fputs("  \x1b[97mHOLD W   ACCELERATE", stdout); break; // 가속 조작이다.
    case 14: fputs("  \x1b[97mA / D    STEER", stdout); break; // 조향 조작이다.
    case 15: fputs("  \x1b[37mRELEASE W TO COOL", stdout); break; // 과열을 피하는 방법이다.
    case 17: fputs("  \x1b[37mR  START / RETRY", stdout); break; // 시작과 재도전이다.
    case 18: fputs("  \x1b[37mQ  QUIT", stdout); break; // 종료 조작이다.
    default: break; // 빈 줄도 유지한다.
    } // 줄 선택 끝.
} // 계기판 끝.

void Render(const Game *game) // 상태를 화면으로 바꾸는 Render 단계이다.
{ // 프레임 그리기 시작.
    int scroll = (int)(game->distance / 6); // 이동 거리에 맞춰 도로 무늬가 흐른다.
    for (int y = 0; y < HEIGHT; ++y) // 배경의 각 줄이다.
        for (int x = 0; x < WIDTH; ++x) // 배경의 각 칸이다.
        { // 배경 색 결정 시작.
            int stripe = (y - scroll) % 4 == 0; // 도로가 플레이어 쪽으로 움직이는 무늬이다.
            int color = ((y - scroll) % 6 == 0) ? LEAF : GRASS; // 도로 옆 잔디이다.
            if (x >= 4 && x <= 25) color = ROAD; // 아스팔트이다.
            if (x == 4 || x == 25) color = stripe ? RED : WHITE; // 양쪽 연석이다.
            if ((x == 11 || x == 18) && stripe) color = LINE; // 끊어진 차선이다.
            pixels[y][x] = color; // 배경 픽셀을 저장한다.
        } // 배경 색 결정 끝.
    float remaining = game->nextFuel - game->distance; // 보급선까지 남은 거리이다.
    if (remaining >= 0 && remaining <= 102) // 가까워지면 도로 전체에 보급선을 그린다.
    { // 보급선 그리기 시작.
        int y = 17 - (int)(remaining / 6); // 플레이어 위치로 다가오는 줄이다.
        for (int x = 5; x < 25; ++x) Pixel(x, y, x % 2 ? 51 : WHITE); // 청록색 체크 무늬이다.
    } // 보급선 그리기 끝.
    for (int i = 0; i < TRAFFIC; ++i) // 활성 장애물을 그린다.
        if (game->traffic[i].active) Car(game->traffic[i].lane, 17 - (int)(game->traffic[i].ahead / 6), RED); // 빨간 장애물이다.
    Car(game->lane, 17, BLUE); // 플레이어는 파란 차이다.
    if (game->phase == OVERHEATED || game->phase == CRASHED) // 실패 원인을 픽셀 폭발로 표시한다.
        for (int dy = -2; dy <= 2; ++dy) // 폭발의 세로 범위이다.
            for (int dx = -2; dx <= 2; ++dx) // 폭발의 가로 범위이다.
                Pixel(8 + game->lane * 7 + dx, 17 + dy, (dx + dy) % 2 ? RED : GOLD); // 붉은 불꽃이다.
    fputs("\x1b[H\x1b[0m\x1b[48;5;234m\x1b[96m  NEON HIGHWAY   /   KEEP COOL. KEEP MOVING.\x1b[K\n\n", stdout); // 제목과 커서 원점이다.
    for (int y = 0; y < HEIGHT; ++y) // 화면의 각 줄을 출력한다.
    { // 출력 줄 시작.
        int previousColor = -1; // 같은 색이 이어지면 ESC 출력을 줄인다.
        for (int x = 0; x < WIDTH; ++x) // 한 줄의 픽셀이다.
        { // 픽셀 출력 시작.
            if (pixels[y][x] != previousColor) printf("\x1b[48;5;%dm", pixels[y][x]); // 배경색 선택이다.
            fputs("  ", stdout); // 공백 두 칸이 정사각형에 가까운 픽셀이다.
            previousColor = pixels[y][x]; // 마지막 색을 기억한다.
        } // 픽셀 출력 끝.
        fputs("\x1b[0m\x1b[48;5;234m", stdout); // 계기판 배경이다.
        Dashboard(game, y); // 같은 줄의 오른쪽 계기판이다.
        fputs("\x1b[K\n", stdout); // 이전 프레임의 긴 문구 잔상을 지운다.
    } // 화면 출력 끝.
    fputs("\x1b[97m\x1b[K\n  ", stdout); // 하단 안내 위치이다.
    if (game->phase == READY) fputs("PRESS R TO START   |   DODGE RED CARS. CROSS THE CYAN FUEL LINE.", stdout); // 대기 화면이다.
    else if (game->phase != RUNNING || game->flash > 0) fputs(notice, stdout); // 실패 또는 보급 안내이다.
    else fputs(game->heat >= 0.8f ? "ENGINE WARNING! RELEASE W!" : "BLUE CAR / RED TRAFFIC / CYAN FUEL POINT", stdout); // 주행 안내이다.
    fputs("\x1b[K\x1b[0m", stdout); // 남은 글자와 색을 정리한다.
    fflush(stdout); // 버퍼에 만든 한 프레임을 한 번에 전달한다.
} // 렌더링 끝.
