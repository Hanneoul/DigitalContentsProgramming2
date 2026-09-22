/*
================================================================================
 [C Language Key Concept: Array & String]

 1. Array란 무엇인가?
	- 수건장에 칸이 3개 있다고 해보자.
	  각 칸에 번호표(Index)가 0, 1, 2 이렇게 붙어 있음.
	- Array는 "같은 타입(Type)의 데이터 여러 개를 연속된 메모리 칸에 모아둔 상자 세트"임.
	- Index는 항상 '0'부터 시작함 (0-based Indexing).

 2. Initialization (초기화) 규칙
	- { 0 }: 첫 번째 칸에 0을 넣고, 나머지 빈 칸도 전부 '0'으로 채움.
	- { 1 }: 첫 번째 칸에 1을 넣고, "나머지 빈 칸은 0"으로 채움 (1로 다 채워지지 않음!).
	- { 1, 2, }: 끝에 붙는 쉼표(Trailing Comma)는 문법적으로 허용되며 결과에 영향 없음.
	- 초기화 없이 선언만 한 변수/Array(예: arr9)에는 memory에 남아있던 "Garbage Value(쓰레기값)"가 들어있음.
	- 크기를 비워둔 경우(예: arr10[]): { } 안에 넣은 데이터 개수에 맞춰 Compiler가 자동으로 크기를 계산함.

 3. String (문자열)과 Null Character
	- C언어에서 String은 '문자(char)들의 Array'임.
	- 컴퓨터는 어디가 문장의 끝인지 알 수 없기 때문에, 문자열 끝에 반드시
	  'Null Character' ('\0', ASCII 값 0)를 붙여서 "여기서 글자가 끝남"을 알림.
	- Double Quotes("K2rea")를 사용하면 Compiler가 자동으로 끝에 '\0'을 추가해 줌.
	- Single Quote('K')로 직접 문자를 나열할 때는 맨 끝에 '\0'을 직접 써주어야 함.
	- 만약 Null Character가 없으면? printf가 글자의 끝을 못 찾고 memory를 계속 읽어서
	  이상한 외계어(Garbage Value)가 함께 출력됨!
================================================================================
*/

#include <stdio.h>

int main()
{
	// --- [PART 1] Fixed-Size Array & Zero Initialization ---
	int arr1[3] = { 0 };       // [0, 0, 0] : 첫 요소에 0 입력 -> 나머지 요소도 모두 0으로 Fill
	int arr2[3] = { 0, };      // [0, 0, 0] : Trailing Comma(끝 쉼표)가 있어도 arr1과 완전히 동일함

	// --- [PART 2] Partial Initialization (부분 초기화) ---
	int arr3[3] = { 1 };       // [1, 0, 0] : 첫 요소만 1, 지정하지 않은 나머지 칸은 전부 '0'으로 채워짐!
	int arr4[3] = { 1, };      // [1, 0, 0] : Trailing Comma 허용, arr3과 동일하게 동작함

	int arr5[3] = { 1, 2 };    // [1, 2, 0] : 0, 1번 Index에 1, 2가 들어가고 남은 2번 Index는 0으로 채워짐
	int arr6[3] = { 1, 2, };   // [1, 2, 0] : arr5와 동일함

	// --- [PART 3] Full Initialization ---
	int arr7[3] = { 1, 2, 3 };  // [1, 2, 3] : 지정한 Size(3)만큼 모든 Element를 직접 채움
	int arr8[3] = { 1, 2, 3, }; // [1, 2, 3] : arr7과 동일함

	// --- [PART 4] Uninitialized Array (초기화하지 않은 배열) ---
	int arr9[3];               // [Garbage Value, ...] : 초기화 안 함! Memory에 남아있던 쓰레기값이 들어있음

	// --- [PART 5] Implicit Size Array (크기 자동 측정) ---
	int arr10[] = { 1 };       // Size = 1 : Element가 1개이므로 Compiler가 int arr10[1]로 자동 처리
	int arr11[] = { 1, };      // Size = 1 : Trailing Comma가 있어도 Element 개수는 1개로 인식됨

	int arr12[] = { 1, 2 };    // Size = 2 : Element가 2개이므로 int arr12[2]로 자동 지정됨
	int arr13[] = { 1, 2, };   // Size = 2 : arr12와 동일함

	// --- [PART 6] String Initialization Techniques ---
	// Method A: char Array 형태로 하나씩 넣기 (Null Character '\0'을 수동으로 명시)
	char s1[] = { 'K', '4', 'r', 'e', 'a', '\0' }; // Size = 6 bytes ('K','4','r','e','a','\0')

	// Method B: String Literal을 { } 안에 넣기
	char s2[] = { "K3rea" };                      // Size = 6 bytes ("K3rea" + 자동 '\0')

	// Method C: String Literal 직접 대입 (가장 흔하게 쓰는 표준 스타일)
	char s3[] = "K2rea";                          // Size = 6 bytes ("K2rea" + 자동 '\0')

	// %s는 Memory에서 '\0'을 만날 때까지 문자를 계속 출력함
	printf("%s %s %s\n", s1, s2, s3); // 출력 결과: K4rea K3rea K2rea

	// --- [PART 7] Dangerous Case: Missing Null Character ---
	// '\0'이 없는 위험한 char Array 선언 (String이 아닌 순수 char Array 상태)
	char s4[] = { 'K', 'o', 'r', 'e', 'a' };      // Size = 5 bytes (Null Character 없음!)

	// 정상적으로 '\0'을 포함한 String 선언
	char s5[] = { 'K', '1', 'r', 'e', 'a', '\0' }; // Size = 6 bytes

	// [주의!] s4에는 '\0'이 없어서 printf가 Memory 상에서 s4 바로 뒤의 데이터를 계속 읽어나감.
	// 실행 환경/컴파일러에 따라 s4 출력 뒤에 쓰레기값이 붙거나, 메모리 근처에 있던 s5의 일부/전체가 연이어 출력될 수 있음.
	printf("%s\n", s4); // Undefined Behavior (의도치 않은 쓰레기값 출력 가능)
	printf("%s\n", s5); // 정상 출력: K1rea

	return 0;
}