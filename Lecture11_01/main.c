//Game Loop
/*
[ Game Loop 시작 ]
          |
          v
+-----------------------+
|  1. 입력 (Input)      | <--- 키보드, 마우스, 조이스틱 신호 수집
+-----------------------+
          |
          v
+-----------------------+
|  2. 업데이트 (Update) | <--- 캐릭터 이동, 충돌 체크, 점수 계산 (데이터 변경)
+-----------------------+
          |
          v
+-----------------------+
|  3. 렌더링 (Render)   | <--- 변경된 데이터를 화면에 그리기 (printf, 그래픽 출력)
+-----------------------+
          |
          v
 [ 종료 조건 검사? (ESC) ] --- 아니오 ---> (다시 입력 단계로 이동)
          |
         예
          |
          v
  [ Game Loop 종료 ]

 * =========================================================================
 * [학습 주제: 게임 개발의 심장, '게임 루프(Game Loop)']
 * =========================================================================
 * 1. 게임 루프란?
 * - 사용자의 입력이 없어도 멈추지 않고 실시간으로 돌아가는 무한 반복문.
 * - 일반 프로그램이 '대기(Wait)' 중심이라면, 게임은 '흐름(Flow)' 중심임.
 *
 * 2. 게임 루프의 3대 핵심 구조 (The Big Three)
 * 1) 입력(Input): 사용자가 무엇을 눌렀는지 확인 (키보드, 마우스 등)
 * 2) 업데이트(Update): 캐릭터 이동, 충돌, 점수 등 게임 내 세상의 수치를 계산
 * 3) 렌더링(Render): 계산된 수치를 바탕으로 화면을 새로 그림 (printf, Draw)
 *
 * 3. 왜 이렇게 짜야 하는가? (구조화의 필요성)
 * - 코드가 섞이면 나중에 복잡한 게임을 만들 때 수정이 불가능해짐.
 * - "듣고(Input), 생각하고(Update), 말하기(Render)"를 분리하는 것이 핵심.
 *
 * [텍스트 구조 다이어그램]
 * While(게임이 실행 중인가?) {
 * [입력] -> 키 입력을 변수에 저장
 * [처리] -> 변수값 계산 (x = x + 1 등)
 * [출력] -> 화면 지우기 후 새로운 위치에 출력
 * }
 * =========================================================================
 */

#include <stdio.h>
#include <Windows.h>

#define GAMESTATE_QUIT -1
#define GAMESTATE_START 0
#define GAMESTATE_TITLE 1
#define GAMESTATE_TITLE_ENTER 2
#define GAMESTATE_TITLE_EXIT 3

 //함수 미리 정의
int ProcessInput();
int Update();
int Render();

/*
 * -1: 게임종료
 * 0 : 게임 시작
 * 1 : 게임 타이틀 화면
 * 2 : 게임 타이틀 화면으로 전환
 * 3 : 게임 타이틀 화면에서 탈출
 */
int gameState = 0;

int main()
{
    SetConsoleOutputCP(65001);
    gameState = 0;
    while (gameState != GAMESTATE_QUIT)
    {
        ProcessInput();
        Update();
        Render();
        Sleep(300);
    }
    return 0;
}

int ProcessInput()
{
    if (gameState == GAMESTATE_TITLE)
    {
        int input = getch();
        if (input == 27)		//esc
        {
            gameState = GAMESTATE_TITLE_EXIT;
        }
    }
    return 0;
}

int Update()
{
    if (gameState == GAMESTATE_START)
    {
        gameState = GAMESTATE_TITLE_ENTER;
    }

    return 0;
}

int Render()
{
    if (gameState == GAMESTATE_TITLE_ENTER)
    {
        system("cls");
        puts("             타이틀 화면이다!!!! \n");
        puts("     ESC 키를 누르면 게임이 종료됩니다. \n");
        gameState = GAMESTATE_TITLE;
    }
    if (gameState == GAMESTATE_TITLE)
    {
        printf("\n입력을 하세요(ESC키!)>");

    }
    if (gameState == GAMESTATE_TITLE_EXIT)
    {
        printf(" 타이틀 화면에서 떠납니다.");
        gameState = GAMESTATE_QUIT;
    }
    if (gameState == GAMESTATE_QUIT)
    {
        printf("\n\n 게임을 종료합니다.");

    }

    return 0;
}