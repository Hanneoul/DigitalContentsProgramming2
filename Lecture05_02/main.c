/*
================================================================================
 [C Language Key Concept: Multi-dimensional Array & sizeof Operator]

 1. Multi-dimensional Array (다차원 배열 - 2D Array)
    - 2D Array는 데이터를 '행(Row)'과 '열(Column)' 형태의 격자판(Grid) 구조로 관리함.
    - 선언 형식: type arrayName[Row_Size][Column_Size];
    - 예: arr[7][5] -> 7개의 Row(행), 각 Row마다 5개의 Column(열)을 가짐.
      (총 Element 개수 = 7 * 5 = 35개)
    - Memory 관점: 컴퓨터 메모리는 1차원(선형) 구조임. 따라서 2D Array도
      실제 Memory 상에는 Row 0부터 Row 6까지 '연속적(Contiguous)'으로 일렬로배치됨.

 2. 2D Array Initialization (다차원 배열 초기화)
    - { {0} }: 첫 번째 Row의 첫 번째 Column 요소에 0을 지정함.
    - C언어 규칙상 Array의 일부만 명시적으로 초기화하면, "나머지 모든 요소는 0으로 Fill"됨.
    - 결과적으로 { {0} } 하나만 작성해도 7x5 전체 35개 요소가 모두 0으로 Reset(Clear)됨.

 3. sizeof Operator (sizeof 연산자)
    - [중요] sizeof는 '함수(Function)'가 아니라 Compiler가 계산하는 '연산자(Operator)'임!
    - Return Value: 대상이 Memory에서 차지하는 'Byte 크기(Byte Size)'를 반환함.
    - int 타입은 일반적으로 4 Bytes임.
    - 7x5 int 배열의 Total Size = (7 * 5) * 4 Bytes = 140 Bytes.

 4. Array Length Calculation (배열의 길이/요소 개수 계산)
    - C언어는 Array의 요소 개수를 직접 알려주는 built-in 속성이 없음.
    - 따라서 '전체 Byte 크기'를 '단일 Element의 Byte 크기(sizeof(int))'로 나누어
      총 Element 개수(Length)를 계산하는 테크닉을 사용함.
    - Formula: Total Elements = sizeof(array) / sizeof(element_type)
================================================================================
*/
#include <stdio.h>

int main()
{
    // --- [PART 1] 2D Array Declaration & Zero Initialization ---
    // 7행 5열(총 35개 칸) 크기의 int형 2D Array 선언
    // { {0} } 구문을 사용해 35개 요소 전체를 0으로 완벽하게 초기화(Fill with 0)함
    int arr[7][5] = { {0} };

    // --- [PART 2] Memory Size Calculation (sizeof Operator) ---
    // sizeof(arr): 2D Array 전체가 Memory에서 차지하는 Total Byte Size를 구함
    // int(4 Bytes) * 35개 요소 = 140 Bytes가 반환되어 size 변수에 저장됨
    int size = sizeof(arr);

    // --- [PART 3] Element Count Calculation (Array Length) ---
    // Total Byte Size(140) / Single Element Byte Size(sizeof(int) = 4)
    // 140 / 4 = 35 -> 배열 전체 요소의 개수(Length)를 산출함
    int length = size / sizeof(int);

    // --- [PART 4] Print Results ---
    // \r\n: Carriage Return + Line Feed (줄바꿈 문자의 표준 조합)
    // Output: 
    // 배열의 크기:140
    // 배열의 길이:35
    printf("배열의 크기:%d\r\n배열의 길이:%d", size, length);

    return 0; // 프로그램 정상 종료
}