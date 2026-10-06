/*
================================================================================
 [강의 자료] 동적 할당 3탄: realloc (자취방 평수 넓히기 & 이사)
================================================================================

 1. R은 대체 뭐의 약자일까? (Name Origin)
   - realloc : [Re]-[Alloc]ation (재할당)
   - 이미 빌려서 쓰고 있던 힙(Heap) 메모리의 크기를 '더 크게' 또는 '더 작게' 조절할 때 사용함.

 2. 초간단 현실 비유: 자취방 옆방 트기 vs 원룸 이사
   - 상황: 살다 보니 짐이 늘어나서 3평짜리 방을 5평으로 넓히고 싶음.

   - Scenario A [제자리 확장 (In-place Expansion)]:
     * 마침 옆방이 비어있음!
     * 벽만 허물어서 기존 방 주소 그대로 평수만 5평으로 싹 넓힘. (개이득!)

   - Scenario B [새로운 건물로 이사 (Relocation)]:
     * 옆방에 이미 다른 세입자(다른 데이터)가 살고 있어서 옆으로 넓힐 수 없음!
     * 집주인이 다른 곳에 새로 넓은 5평짜리 방을 구해줌.
     * 기존 방에 있던 짐(데이터)을 새 방으로 전부 '자동 복사'해서 옮겨주고, 옛날 방은 자동으로 반납(free)함.
     * 결과적으로 **메모리의 시작 주소가 완전히 변경됨!**

 3. 문법(사용법)과 최대 주의사항 (★ 시험 & 실무 단골 1위)
   - 문법: void* realloc(void *ptr, size_t new_size);
     * ptr      : 기존에 가지고 있던 메모리 주소
     * new_size : 변경하고 싶은 '총 바이트 크기' (추가할 크기가 아니라 전체 최종 크기임!)

   - [초대형 주의사항] realloc 실패 시 '임시 포인터'를 써야 하는 이유!
     * 잘못된 코드: p = (int*)realloc(p, 1000000000);
     * 만약 메모리가 부족해서 realloc이 실패(NULL 반환)하면?
     * p에 NULL이 들어가면서 **기존에 잘 들고 있던 원본 메모리 주소마저 날아가 버림!** (메모리 이산가족 발생/메모리 누수)
     * 따라서 반드시 **'임시 포인터(temp)'**에 받고, NULL이 아닌 것을 확인한 뒤 p에 대입해야 안전함.
================================================================================
*/

#include <stdio.h>
#include <stdlib.h> // malloc, realloc, free 함수용 헤더

int main(void)
{
    // 1. 처음엔 소소하게 정수 3개(12바이트) 공간만 생성 (malloc)
    int initialSize = 3;
    int* pArray = (int*)malloc(sizeof(int) * initialSize);

    if (pArray == NULL)
    {
        printf("초기 메모리 할당 실패!\n");
        return 1;
    }

    // 초기 데이터 저장
    for (int i = 0; i < initialSize; i++)
    {
        pArray[i] = (i + 1) * 10; // 10, 20, 30
    }

    printf("================ 1. 초기 메모리 상태 (3칸) ================\n");
    printf("기존 메모리 주소: %p\n", (void*)pArray);
    printf("기존 데이터: ");
    for (int i = 0; i < initialSize; i++)
    {
        printf("%d ", pArray[i]);
    }
    printf("\n\n");


    // 2. 갑자기 동료 2명이 추가 입주함! 메모리를 5칸으로 넓혀보자 (realloc)
    int newSize = 5;
    printf("================ 2. realloc으로 5칸으로 확장 중... ================\n");

    // [유머/핵심] 원본 pArray에 바로 받지 않고 pTemp라는 임시 대리인을 내세움!
    // 만약 부동산 계약이 파기(NULL)되더라도 내 옛날 집 주소(pArray)는 지켜야 하기 때문!
    int* pTemp = (int*)realloc(pArray, sizeof(int) * newSize);

    if (pTemp == NULL)
    {
        // 리얼주소 날아갈 뻔함! 임시 포인터를 썼으므로 pArray는 안전함.
        printf("[비상] 메모리 확장 실패! 기존 데이터라도 지킵니다.\n");
        free(pArray); // 옛날 방은 반납하고 안전하게 종료
        return 1;
    }

    // 확장에 성공했으므로 안심하고 진짜 포인터에 대입!
    pArray = pTemp;

    printf("확장 후 메모리 주소: %p (옆방이 비었으면 주소 동일, 찼으면 이사해서 주소 변경됨!)\n", (void*)pArray);

    // 새롭게 늘어난 4번째, 5번째 칸에 데이터 추가
    pArray[3] = 40;
    pArray[4] = 50;

    printf("확장 후 전체 데이터 (기존 10, 20, 30 보존 확인!): ");
    for (int i = 0; i < newSize; i++)
    {
        printf("%d ", pArray[i]);
    }
    printf("\n\n");


    // 3. 반대로 공간을 2칸으로 줄여보기 (realloc 다운사이징)
    printf("================ 3. realloc으로 2칸으로 축소 테스트 ================\n");
    pTemp = (int*)realloc(pArray, sizeof(int) * 2);

    if (pTemp != NULL)
    {
        pArray = pTemp;
        printf("축소 후 데이터 (30, 40, 50은 눈물을 머금고 손절당함): ");
        for (int i = 0; i < 2; i++)
        {
            printf("%d ", pArray[i]);
        }
        printf("\n");
    }


    // 4. 퇴실 및 집주인과의 마지막 인사
    free(pArray);
    pArray = NULL;
    pTemp = NULL;

    printf("\n[퇴실 완료] free()로 깔끔하게 반납 완료!\n");

    return 0;
}