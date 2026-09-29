/*
 ============================================================================
 [ enum(열거형) 핵심 가이드 & 실무 활용 패턴 ]
 ============================================================================

 1. enum(Enumeration : 열거형)이란?
    - 연관된 정수 상수(0, 1, 2, ...)들에 '사람이 읽기 쉬운 이름'을 부여하는 타입.
    - 기본적으로 첫 번째 항목은 0, 이후 항목은 1씩 자동으로 증가함.
    - 예: enum { RED, GREEN, BLUE }; -> RED=0, GREEN=1, BLUE=2

 2. 왜 숫자를 직접 안 쓰고 enum을 쓸까? (가독성 & 타입 안전성)
    - 숫자 '2'가 '포션'인지 '칼'인지 코드를 볼 때 알기 어려움 (Magic Number 문제).
    - ITEM_POTION 처럼 명시적인 이름을 쓰면 코드 해석이 직관적이 됨.

 3. 💥 enum의 최고 장점: ITEM_COUNT 패턴 (자동 확장형 리스트)
    - 열거형의 맨 마지막에 'ITEM_COUNT' 같은 요소를 하나 추가해두면,
      이 값은 **자동으로 현재 전체 아이템 총개수**가 됨!
    - 새로운 아이템(예: ITEM_ELIXIR)을 중간/끝에 추가하기만 하면:
      ① ITEM_COUNT 값이 자동으로 1 증가함.
      ② 배열 크기, for문 루프 범위 등이 코드 수정 없이 알아서 자동으로 확장됨!
 ============================================================================
*/

#include <stdio.h>

// [1] 아이템 종류를 정의하는 enum
// 새 아이템을 넣고 싶으면 ITEM_COUNT 바로 위에 이름만 추가하면 끝!
typedef enum 
{
    ITEM_HP_POTION,   // 0
    ITEM_MP_POTION,   // 1
    ITEM_SWORD,       // 2
    ITEM_SHIELD,      // 3
    ITEM_RING,        // 4 (새 항목 추가 시 여기 넣기만 하면 됨)

    ITEM_COUNT        // 5 -> 아이템의 총 개수를 자동으로 파악하는 핵심 카운터!
} ItemType;

// [2] 아이템 상세 정보를 담는 struct
typedef struct 
{
    char name[30];  // 아이템 이름
    int price;      // 가격
    int weight;     // 무게
} ItemInfo;

// [3] enum 인덱스와 1:1로 매핑되는 마스터 아이템 DB 테이블 (배열 크기를 ITEM_COUNT로 지정)
// enum에 항목이 추가되면 ITEM_COUNT가 늘어나므로 배열 크기도 자동으로 확장됨!
const ItemInfo G_ItemDatabase[ITEM_COUNT] = 
{
    [ITEM_HP_POTION] = {"체력 회복 포션", 50,  1},
    [ITEM_MP_POTION] = {"마나 회복 포션", 70,  1},
    [ITEM_SWORD] = {"강철 단검",     300, 5},
    [ITEM_SHIELD] = {"나무 방패",     200, 8},
    [ITEM_RING] = {"반지",          500, 1}
};

// [4] 플레이어의 인벤토리 (각 아이템 타입별 소지 개수 저장)
typedef struct 
{
    int inventory[ITEM_COUNT]; // ITEM_COUNT 크기만큼 자동으로 인벤토리 슬롯 생성!
} Inventory;

int main(void) 
{
    printf("========================================\n");
    printf(" 1. enum 자동 카운팅(ITEM_COUNT) 확인\n");
    printf("========================================\n");
    printf("현재 등록된 총 아이템 종류: %d개\n\n", ITEM_COUNT);


    printf("========================================\n");
    printf(" 2. 아이템 획득 및 인벤토리 데이터 처리\n");
    printf("========================================\n");

    Inventory myInven = { 0 }; // 인벤토리 0으로 초기화

    // 숫자가 아닌 enum 이름(ITEM_HP_POTION 등)을 직접 써서 직관적으로 아이템 추가!
    myInven.inventory[ITEM_HP_POTION] += 5; // HP 포션 5개 획득
    myInven.inventory[ITEM_SWORD] += 1; // 칼 1개 획득
    myInven.inventory[ITEM_RING] += 2; // 반지 2개 획득

    // 아이템 이름을 통해 마스터 DB에서 직접 정보 조회
    printf("[%s] %d개 보유 중! (개당 가격: %d골드)\n",
        G_ItemDatabase[ITEM_SWORD].name,
        myInven.inventory[ITEM_SWORD],
        G_ItemDatabase[ITEM_SWORD].price);

    printf("[%s] %d개 보유 중! (개당 가격: %d골드)\n\n",
        G_ItemDatabase[ITEM_HP_POTION].name,
        myInven.inventory[ITEM_HP_POTION],
        G_ItemDatabase[ITEM_HP_POTION].price);


    printf("========================================\n");
    printf(" 3. 자동 루프 순회 (코드 수정 없이 하드캐리되는 부분)\n");
    printf("========================================\n");
    printf("--- 전체 인벤토리 현황 출력 ---\n");

    // i < ITEM_COUNT 로 돌리기 때문에, enum에 아이템이 100개로 늘어나도
    // 이 출력 함수는 코드를 단 한 줄도 수정할 필요가 없음!
    int totalPrice = 0;
    int totalWeight = 0;

    for (int i = 0; i < ITEM_COUNT; i++) {
        int count = myInven.inventory[i];
        if (count > 0) {
            int itemTotalPrice = G_ItemDatabase[i].price * count;
            int itemTotalWeight = G_ItemDatabase[i].weight * count;

            printf(" - %-15s | 수량: %2d개 | 합계금액: %5d골드 | 총무게: %2dkg\n",
                G_ItemDatabase[i].name, count, itemTotalPrice, itemTotalWeight);

            totalPrice += itemTotalPrice;
            totalWeight += itemTotalWeight;
        }
    }

    printf("----------------------------------------\n");
    printf("총 가치: %d골드 | 총 자산 무게: %dkg\n", totalPrice, totalWeight);

    return 0;
}