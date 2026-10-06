/*
================================================================================
 [강의 자료] 동적 할당 2탄: malloc vs calloc (청소 안 된 방 vs 입주 청소 완료)
================================================================================

 1. C는 대체 뭐의 약자일까? (Name Origin)
   - malloc : [M]emory Allocation (메모리 할당)
   - calloc : [C]leared Allocation (또는 Contiguous Allocation)
     * 공식 명칭은 Contiguous(연속적인)이지만, 현업과 강의에서는 메모리를 싹 비워준다는
       meaning에서 [C]leared Allocation(청소된 할당)으로 외우는 게 백배 유효함!

 2. 초간단 현실 비유: 자취방 임대 계약
   - malloc : "월세 싸게 줄 테니 그냥 들어와라." (이전 세입자가 쓰던 쓰레기가 그대로 남은 방)
              * 메모리를 빌려만 오고 안을 청소하지 않아서 '쓰레기 값(Garbage Value)'이 들어있음.
              * 속도가 약간 빠름 (청소를 안 하니까!).

   - calloc : "보증금 좀 더 받고 입주 청소 완벽하게 해둠." (모든 방을 '0'으로 깨끗이 초기화)
              * 메모리를 빌려오자마자 모든 바이트를 0으로 싹 밀어버림.
              * 안전하지만 아주 미세하게 시간이 더 걸림 (청소하느라!).

 3. 문법(사용법)의 차이 (★ 시험 및 실무 단골)
   - malloc : 전체 필요한 '바이트 크기' 1개를 인자로 받음.
              int *p = (int*)malloc(sizeof(int) * 5); // 4바이트 * 5개 = 20바이트 주세요!

   - calloc : '개수'와 '하나당 크기' 2개를 나열해서 받음.
              int *p = (int*)calloc(5, sizeof(int));   // 4바이트짜리 5개 만들어서 청소해 주세요!

 4. 핵심 정리: 언제 뭘 써야 할까?
   - "어차피 내가 바로 다른 값으로 덮어씌울 거다" -> malloc 쓰면 됨.
   - "초기값이 0이어야 안전하다 (예: 스코어 board, 맵 초기화, 카운팅)" -> calloc 쓰면 됨.
================================================================================
*/

#include <stdio.h>
#include <stdlib.h> // malloc, calloc, free 함수를 위한 헤더

int main(void)
{
    int count = 5;

    printf("================ 1. malloc (청소 안 된 쓰레기 방) ================\n");
    // malloc: 5개 정수 공간 할당 (내부는 청소되지 않음!)
    int* pMalloc = (int*)malloc(sizeof(int) * count);

    if (pMalloc == NULL)
    {
        printf("malloc 할당 실패!\n");
        return 1;
    }

    printf("[malloc 결과물] 청소를 안 하고 들어갔더니 보관함에 들어있는 값:\n");
    for (int i = 0; i < count; i++)
    {
        // [유머] 매번 실행할 때마다 혹은 컴파일러 상태에 따라 기괴한 쓰레기 값이 출력됨!
        printf("  pMalloc[%d] = %d (<- 이전 세입자가 버리고 간 양말)\n", i, pMalloc[i]);
    }


    printf("\n================ 2. calloc (입주 청소 완료된 깔끔한 방) ================\n");
    // calloc: 5개 정수 공간 할당 + 0으로 완벽 청소!
    int* pCalloc = (int*)calloc(count, sizeof(int));

    if (pCalloc == NULL)
    {
        printf("calloc 할당 실패!\n");
        free(pMalloc); // pMalloc은 먼저 해제해주는 매너
        return 1;
    }

    printf("[calloc 결과물] 입주 청소업체가 싹 비워둔 보관함의 값:\n");
    for (int i = 0; i < count; i++)
    {
        // calloc은 모든 칸을 '0'으로 확실하게 고장 없이 초기화해 줌!
        printf("  pCalloc[%d] = %d (<- 피자 한 조각도 안 남고 보송보송함)\n", i, pCalloc[i]);
    }


    // =========================================================================
    // ★ [매우 중요] 반납 타임 (자취방 퇴실)
    // =========================================================================
    // malloc이든 calloc이든 빌려온 힙 메모리는 무조건 free()로 반납해야 함!
    // 반납 안 하면 집주인(OS)이 분노하며 메모리 누수(Memory Leak)라는 벌금을 물림.
    free(pMalloc);
    free(pCalloc);

    // 댕글링 포인터 방지를 위한 NULL 초기화
    pMalloc = NULL;
    pCalloc = NULL;

    printf("\n[퇴실 완료] free()로 두 방 모두 깔끔하게 반납했음. 깔끔한 개발자 인정!\n");

    return 0;
}