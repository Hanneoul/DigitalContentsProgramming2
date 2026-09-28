/*
================================================================================
 [강의 자료] 2차원 배열(2D Array)의 이해와 활용: 2D 텍스트 RPG 맵
================================================================================

 1. 2차원 배열이란 무엇인가?
   - 1차원 배열을 행(Row)과 열(Column) 형태로 확장한 형태임 (바둑판, 표 구조).
   - 선언 방식: 자료형 배열이름[행_크기][열_크기];
   - 예: char map[5][5]; -> 총 5개의 행, 각 행당 5개의 열 (총 25개의 칸 생성).

 2. 2차원 배열의 메모리 구조와 접근 방법
   - 컴퓨터 메모리는 1차원이므로, 2차원 배열도 실제로 메모리에는 1차원으로 연속 배치됨.
   - 접근 시 인덱스를 2개 사용함: map[y][x] 또는 map[row][col]
     - 첫 번째 인덱스 [y]: 행(Row) 위치 -> 세로 방향 (위/아래)
     - 두 번째 인덱스 [x]: 열(Column) 위치 -> 가로 방향 (왼쪽/오른쪽)

 3. 화면 좌표계(Screen Coordinate System)의 특성 (★ 헷갈리기 쉬움)
   - 일반 수학 평면 좌표계: Y축이 위로 갈수록 증가함.
   - 모니터/컴퓨터 화면 좌표계: Y축이 아래로 갈수록 증가함.
     - (0, 0) : 맨 왼쪽 위 칸
     - w 키 (위로 이동)   : Y 좌표가 '감소'함 (playerY--)
     - s 키 (아래로 이동) : Y 좌표가 '증가'함 (playerY++)
     - a 키 (왼쪽 이동)   : X 좌표가 '감소'함 (playerX--)
     - d 키 (오른쪽 이동) : X 좌표가 '증가'함 (playerX++)

 4. 중첩 for 문(Nested Loop)을 활용한 2차원 배열 출력
   - 2차원 배열을 출력하려면 외부 for 문(행 접근)과 내부 for 문(열 접근)이 필수적임.
   - 바깥쪽 loop가 Y좌표(세로)를 순회할 때, 안쪽 loop가 X좌표(가로)를 순회하며 맵을 그려냄.
================================================================================
*/

#include <stdio.h>
#include <conio.h>  // _getch() 함수 사용

#define MAP_HEIGHT 5 // 맵의 세로 크기 (행, Row)
#define MAP_WIDTH  5 // 맵의 가로 크기 (열, Column)

int main(void)
{
    // 5x5 크기의 2차원 문자 배열 선언
    char map[MAP_HEIGHT][MAP_WIDTH];

    // 플레이어의 초기 좌표 (중앙에 위치: y=2, x=2)
    int playerY = 2;
    int playerX = 2;

    // 입력 저장용 변수
    char input = 0;

    // 1. 2차원 배열 초기화 (중첩 for 문 활용)
    // 세로 방향(행)을 먼제 순회
    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        // 가로 방향(열)을 순회
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            map[y][x] = '.'; // 모든 격자를 빈 공간('.')으로 초기화
        }
    }

    // 게임 루프
    while (1)
    {
        system("cls");
        // 2. 현재 플레이어 위치 업데이트 (2차원 인덱스 지정 [Y][X])
        map[playerY][playerX] = '0';

        // 3. 화면 렌더링 (중첩 for 문을 이용한 맵 출력)
        // system("cls"); // 화면 지우기 (필요시 주석 해제)
        printf("\n=== 2D RPG MAP (5x5) ===\n");
        for (int y = 0; y < MAP_HEIGHT; y++)
        {
            for (int x = 0; x < MAP_WIDTH; x++)
            {
                printf("%c ", map[y][x]); // 한 문자씩 띄어서 보기 좋게 출력
            }
            printf("\n"); // 한 행(Row) 출력이 끝나면 줄바꿈
        }

        printf("이동: [w] 위 | [s] 아래 | [a] 왼쪽 | [d] 오른쪽 | [q] 종료\n");
        printf("현재 위치: (Y: %d, X: %d)\n", playerY, playerX);
        printf("입력: ");

        // 4. 입력 받기
        input = _getch();

        if (input == 'q' || input == 'Q')
        {
            printf("\n게임을 종료함.\n");
            break;
        }

        // 5. 이동 전 이전 위치 원복
        map[playerY][playerX] = '.';

        // 6. WSAD 키 처리 및 2차원 경계 검사
        if (input == 'w' || input == 'W') // 위로 이동 (Y 감소)
        {
            if (playerY > 0)
            {
                playerY--;
            }
            else
            {
                printf("\n[경고] 위쪽 벽에 부딪혔음!\n");
            }
        }
        else if (input == 's' || input == 'S') // 아래로 이동 (Y 증가)
        {
            if (playerY < MAP_HEIGHT - 1)
            {
                playerY++;
            }
            else
            {
                printf("\n[경고] 아래쪽 벽에 부딪혔음!\n");
            }
        }
        else if (input == 'a' || input == 'A') // 왼쪽으로 이동 (X 감소)
        {
            if (playerX > 0)
            {
                playerX--;
            }
            else
            {
                printf("\n[경고] 왼쪽 벽에 부딪혔음!\n");
            }
        }
        else if (input == 'd' || input == 'D') // 오른쪽으로 이동 (X 증가)
        {
            if (playerX < MAP_WIDTH - 1)
            {
                playerX++;
            }
            else
            {
                printf("\n[경고] 오른쪽 벽에 부딪혔음!\n");
            }
        }
    }

    return 0;
}