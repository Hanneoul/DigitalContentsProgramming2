/*
 ============================================================================
 [ struct(구조체) vs union(공용체) 완벽 비교 및 작동 원리 ]
 ============================================================================

 1. struct (구조체)란?
    - 서로 다른 타입의 데이터들을 하나로 묶어 다루는 '독자 메모리 집합체'.
    - 선언된 모든 멤버 변수가 각자 전용 메모리 공간을 할당받음.
    - 특징: 모든 멤버 변수를 동시에 저장하고 안전하게 읽고 쓸 수 있음.
    - 전체 크기: 모든 멤버 크기의 합 + 메모리 패딩(Padding Byte).

 2. union (공용체)란?
    - 서로 다른 타입의 데이터들이 '하나의 메모리 공간을 공유'하는 타입.
    - 선언된 모든 멤버 변수의 시작 주소(&)가 완전히 동일함.
    - 특징: 한 시점에는 단 하나의 멤버 변수만 의미 있는 값을 가짐.
            어느 한 멤버의 값을 변경하면 다른 멤버의 값도 덮어씌워져 파괴됨.
    - 전체 크기: 가장 크기가 큰 멤버 변수의 크기로 결정됨.

 3. 왜 union(공용체)을 사용하는가?
    ① 메모리 절약: 서로 동시에 사용되지 않는 속성들을 하나로 묶어 공간 절약.
       (예: 게임 아이템 - 무기는 공격력만, 방어구는 방어력만 필요한 경우)
    ② 데이터 재해석: 4바이트 정수 하나를 1바이트 4개 패킷으로 나누어 보낼 때.

 4. 메모리 할당 비교 (int 4바이트, char 1바이트 4개 기준)
    - struct Data  : [int 4byte] [char 4byte]  => 총 8바이트 (각자 영역 소유)
    - union Data   : [ int / char[4] 공유 ]     => 총 4바이트 (동일 주소 공유)
 ============================================================================
*/

#include <stdio.h>

// [1] 동일한 멤버를 가진 struct 정의
typedef struct {
    int iValue;      // 4바이트 정수
    char bytes[4];   // 1바이트 4개 배열 (총 4바이트)
} StructData;

// [2] 동일한 멤버를 가진 union 정의
typedef union {
    int iValue;      // 4바이트 정수
    char bytes[4];   // 1바이트 4개 배열 (총 4바이트) -> iValue와 메모리 공유!
} UnionData;

int main(void) {
    printf("========================================\n");
    printf(" 1. 메모리 크기(sizeof) 및 주소 비교\n");
    printf("========================================\n");

    StructData sData;
    UnionData uData;

    // struct는 멤버별로 메모리를 따로 가지므로 크기가 합산됨 (8바이트)
    printf("[struct] 전체 크기: %zu 바이트\n", sizeof(StructData));
    printf("         iValue 주소: %p\n", (void*)&sData.iValue);
    printf("          bytes 주소: %p\n\n", (void*)&sData.bytes);

    // union은 모든 멤버가 시작 주소를 공유하므로 크기가 4바이트로 동일함
    printf("[union]  전체 크기: %zu 바이트 (가장 큰 멤버 기준)\n", sizeof(UnionData));
    printf("         iValue 주소: %p\n", (void*)&uData.iValue);
    printf("          bytes 주소: %p (iValue와 주소가 완전히 같음!)\n\n", (void*)&uData.bytes);


    printf("========================================\n");
    printf(" 2. 값 변경 시 다른 멤버에 미치는 영향\n");
    printf("========================================\n");

    // --- [struct 실험] ---
    sData.iValue = 0x12345678;
    sData.bytes[0] = 0xFF; // 'A' (ASCII 65) 대입

    printf("[struct] iValue에 100을 넣고 bytes[0]에 '0xFF'를 넣었을 때:\n");
    printf("         sData.iValue   = 0x%X (값 보존됨)\n", sData.iValue);
    printf("         sData.bytes[0] = 0x%X (값 보존됨)\n\n", (unsigned char)sData.bytes[0]);

    // --- [union 실험: 데이터 덮어쓰기 현상] ---
    uData.iValue = 0x12345678; // 16진수 정수값 대입

    printf("[union]  uData.iValue = 0x12345678 대입 직후:\n");
    printf("         uData.iValue = 0x%X\n", uData.iValue);
    // 동일한 메모리를 공유하므로 iValue의 각 바이트를 bytes 배열로 즉시 접근 가능
    printf("         uData.bytes 내부 1바이트별 값: 0x%X, 0x%X, 0x%X, 0x%X\n\n",
        (unsigned char)uData.bytes[0], (unsigned char)uData.bytes[1],
        (unsigned char)uData.bytes[2], (unsigned char)uData.bytes[3]);

    // union 멤버 하나를 변경하면 덮어씌워져서 다른 멤버 값도 변함!
    uData.bytes[0] = 0xFF; // 가장 첫 바이트를 0xFF로 변경

    printf("[union]  uData.bytes[0] = 0xFF 로 변경 후:\n");
    printf("         uData.bytes[0] = 0xFF\n");
    printf("         uData.iValue   = 0x%X (iValue 값까지 함께 오염/변경됨!)\n", uData.iValue);

    return 0;
}