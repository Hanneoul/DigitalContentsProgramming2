/*
 ============================================================================
 [ C언어 구조체(struct) 핵심 문법 및 초기화 총정리 가이드 ]
 ============================================================================

 1. typedef 키워드의 의미와 역할
    ------------------------------------
    - 'Type Definition'의 줄임말로, 기존 데이터 타입에 '새로운 별칭(Alias)'을 부여함.
    - 구조체 선언 시 매번 'struct' 키워드를 붙여야 하는 번거로움을 줄여줌.
      예: typedef unsigned int UINT;  // unsigned int 대신 UINT 사용 가능

 2. struct 변수 {}; vs struct {} 변수; 의 차이점
    ------------------------------------
    ① struct 태그명 { ... };
       - 구조체의 '타입(설계도)'만 정의하는 구문.
       - {} 앞의 이름은 변수가 아니라 구조체 타입 이름(Tag)이며, 메모리가 할당되지 않음.
       - 사용 시: struct 태그명 변수명; (별도 변수 선언 필요)

    ② struct { ... } 변수명;
       - 구조체 타입을 정의함과 동시에 '실제 변수'를 즉시 생성하는 구문.
       - {} 뒤의 이름은 메모리를 차지하는 '변수 이름'임.
       - 태그명을 생략(익명 구조체)하면 나중에 이 타입으로 새 변수를 추가 선언할 수 없음.

 3. 구조체 정의 끝에 세미콜론(;)을 반드시 찍어야 하는 이유
    ------------------------------------
    - C언어 문법 구조상 "타입 정의"와 "변수 선언"을 한 문장으로 작성할 수 있음.
      예: struct Point { int x, y; } p1, p2; // 정의 + 변수 선언
    - 컴파일러에게 "변수 선언 없이 구조체 타입 정의 문장이 여기서 완벽히 끝났다"는
      종료 신호를 알려주기 위해 끝에 세미콜론(;)이 필수적임.

 4. 구조체 정의 스타일 3가지 비교
    ------------------------------------
    [스타일 A] typedef 없는 전통적인 방식 (매번 struct 키워드 필요)
      struct Player {
          int hp;
          int atk;
      };
      struct Player p1; // 변수 선언

    [스타일 B] typedef를 사용하는 방식 (가장 많이 사용됨)
      typedef struct {
          int hp;
          int atk;
      } Player;
      Player p1; // struct 생략 가능

    [스타일 C] 이름 없는 익명 구조체 (1회성 변수 단발성 선언)
      struct {
          int hp;
          int atk;
      } player1, player2; // 선언과 동시에 변수 생성

 5. 중괄호 {}를 이용한 초기화와 대입의 중요한 차이
    ------------------------------------
    - 초기화 (Initialization): 변수를 '선언하는 시점'에만 {}를 사용 가능.
      예: SType s = { 1, 2, 3.14 }; // 정상 작동

    - 대입 (Assignment): 이미 선언된 변수에는 {}를 직접 대입할 수 없음 (문법 오류).
      예: s = { 1, 2, 3.14 };       // 오류 발생!

    - 이미 선언된 구조체 변수 값을 변경하는 방법:
      1) 멤버별 직접 대입: s.m1 = 1; s.m2 = 2; s.m3 = 3.14;
      2) 복합 리터럴(C99 이상): s = (SType){ 1, 2, 3.14 };
 ============================================================================
*/

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

// [1] Enemy 구조체 정의 (typedef 사용)
typedef struct 
{
    char name[20]; // 적의 이름
    int hp;        // 체력
    int atk;       // 공격력
    int isAlive;   // 생사 여부 (1: 살아있음, 0: 사망)
} Enemy;

// [2] Player 구조체 정의 (typedef 사용)
typedef struct 
{
    char name[20]; // 플레이어 이름
    int hp;        // 체력
    int atk;       // 공격력
} Player;

// [3] Player가 Enemy를 공격하는 함수 정의
// enemy의 HP 및 isAlive 상태를 실제로 변경해야 하므로 포인터(*)로 전달함
void PlayerAttack(Player* attacker, Enemy* target) 
{
    // 공격 대상이 이미 사망한 경우 공격 처리 스킵
    if (target->isAlive == 0) 
    {
        printf("[알림] %s은(는) 이미 처치된 적입니다!\n", target->name);
        return;
    }

    printf("\n     %s이(가) %s을(를) 공격합니다! (데미지: %d)\n", attacker->name, target->name, attacker->atk);

    // 적의 체력 감축
    target->hp -= attacker->atk;

    // 체력이 0 이하가 되었는지 확인하여 처리
    if (target->hp <= 0) {
        target->hp = 0;       // 체력이 음수가 되지 않도록 0으로 고정
        target->isAlive = 0;  // 생사 여부 속성값을 0(사망)으로 변경
        printf("   %s의 체력이 0이 되어 처치되었습니다!\n", target->name);
    }
    else {
        printf("   %s의 남은 체력: %d\n", target->name, target->hp);
    }
}

int main(void) {
    // [Player 생성 및 사용자 입력 초기화]
    Player player; // Player 구조체 변수 선언

    printf("=== 플레이어 정보 입력 ===\n");
    printf("플레이어 이름 입력: ");
    scanf_s("%s", player.name, (int)sizeof(player.name)); // 이름 입력받기

    printf("플레이어 체력(HP) 입력: ");
    scanf_s("%d", &player.hp, (int)sizeof(player.hp)); // 체력 입력받기

    printf("플레이어 공격력(ATK) 입력: ");
    scanf("%d", &player.atk); // 공격력 입력받기

    // [Enemy 3명 생성 및 중괄호 {} 초기화]
    // 구조체 배열 선언과 동시에 각 요소의 멤버를 {} 리터럴로 초기화
    Enemy enemies[3] = {
        {"슬라임", 30, 5, 1},  // enemies[0]: HP 30, ATK 5, isAlive 1
        {"고블린", 50, 10, 1}, // enemies[1]: HP 50, ATK 10, isAlive 1
        {"오크", 100, 20, 1}   // enemies[2]: HP 100, ATK 20, isAlive 1
    };

    printf("\n========================================\n");
    printf("      전투 시작! 적 3명이 나타났습니다.\n");
    printf("========================================\n");

    // [전투 테스트 진행]
    // 1번째 적(슬라임) 공격 -> HP 30이므로 공격력에 따라 전투 진행
    PlayerAttack(&player, &enemies[0]);
    PlayerAttack(&player, &enemies[0]); // 필요시 추가 공격하여 처치 유도

    // 2번째 적(고블린) 공격
    PlayerAttack(&player, &enemies[1]);

    printf("\n========================================\n");
    printf("      전체 적 생사 여부 탐색 결과\n");
    printf("========================================\n");

    // [최종 loop: enemies 배열을 하나씩 검색하여 isAlive 판단]
    for (int i = 0; i < 3; i++) {
        // isAlive 속성을 검사하여 출력
        if (enemies[i].isAlive == 1) {
            printf("[%d] %s - 상태: 생존 (남은 HP: %d)\n", i + 1, enemies[i].name, enemies[i].hp);
        }
        else {
            printf("[%d] %s - 상태: 사망 (HP: %d)\n", i + 1, enemies[i].name, enemies[i].hp);
        }
    }

    return 0;
}