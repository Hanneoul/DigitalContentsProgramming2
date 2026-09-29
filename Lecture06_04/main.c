/*
 ============================================================================
 [ C언어 구조체(struct) 핵심 문법 및 메모리 패딩(Padding) 가이드 ]
 ============================================================================

 1. 구조체 패딩(Structure Padding)과 바이트 정렬(Byte Alignment)이란?
    ------------------------------------
    - 구조체 멤버의 실제 크기 합보다 sizeof(구조체)의 값이 더 크게 나오는 현상.
    - CPU가 메모리에서 데이터를 읽어올 때 성능(속도)을 최적화하기 위해
      멤버와 멤버 사이에 의미 없는 빈 공간(Padding Byte)을 자동으로 채워넣음.

 2. CPU는 왜 패딩(Padding)을 사용할까?
    ------------------------------------
    - Modern CPU(32bit, 64bit)는 메모리를 1바이트 단위가 아니라 4바이트 또는 8바이트
      단위(Bus Width)로 한 번에 접근함.
    - 만약 4바이트 정수(int)가 메모리의 '홀수 번지 주소'에 걸쳐 저장되어 있다면,
      CPU는 이 정수 하나를 읽기 위해 메모리에 2번 접근(2번의 Memory Cycle)해야 함.
    - 따라서 CPU가 한 번의 접근으로 데이터에 즉시 접근(Naturally Aligned Access)할 수
      있도록, 각 데이터 타입의 크기의 배수 주소에 데이터를 위치시키고 남는 공간을 패딩으로 채움.

 3. 패딩 정렬 공식 (Natural Alignment Rule)
    ------------------------------------
    - 각 멤버는 자신 크기의 배수가 되는 메모리 주소(오프셋)에 배치됨.
      (예: char=1바이트 아무데나, short=2바이트 배수, int/float=4바이트 배수, double=8바이트 배수)
    - 구조체의 전체 크기는 '가장 크기가 큰 멤버'의 크기 배수로 최종 맞춤(Padding)됨.

 4. 패딩 메모리 구조 예시
    ------------------------------------
    struct Example {
        char a;    // 1 바이트 (Offset 0)
                   // [패딩 3 바이트 추가] -> int의 시작 주소를 4의 배수로 맞춤
        int b;     // 4 바이트 (Offset 4)
        short c;   // 2 바이트 (Offset 8)
                   // [패딩 2 바이트 추가] -> 전체 크기를 가장 큰 멤버(int=4)의 배수인 12로 맞춤
    };
    - 멤버들의 단순 합: 1 + 4 + 2 = 7 바이트
    - 실제 sizeof(struct Example): 12 바이트! (5바이트가 패딩으로 낭비됨)

    💡 [팁] 멤버를 크기순(큰 타입 -> 작은 타입)으로 배치하면 패딩을 크게 줄일 수 있음!
    struct Optimized {
        int b;     // 4 바이트
        short c;   // 2 바이트
        char a;    // 1 바이트
                   // [패딩 1 바이트 추가]
    };             // sizeof = 8 바이트로 절약됨!
*/

#include <stdio.h>

// [패딩 발생 예시 구조체]
typedef struct {
    char a;    // 1바이트
    int b;     // 4바이트
    short c;   // 2바이트
} UnpackedStruct;

// [패딩 최적화 예시 구조체]
typedef struct {
    int b;     // 4바이트
    short c;   // 2바이트
    char a;    // 1바이트
} PackedStruct;

int main(void) {
    // --- 구조체 패딩 크기 비교 출력 ---
    printf("=== 구조체 패딩 크기 비교 ===\n");
    printf("UnpackedStruct 크기: %zu 바이트 (실제 멤버 합: 7바이트)\n", sizeof(UnpackedStruct));
    printf("PackedStruct 크기:   %zu 바이트 (재배치로 패딩 줄임)\n\n", sizeof(PackedStruct));

    
    return 0;
}