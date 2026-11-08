/*
==============================================================================
 [강의 자료] Lecture12_03 | 콜백 등록 | 같은 상자, 다른 보상
==============================================================================

1. 오늘의 장면: 보물 상자의 보상 버튼
    01 함수 주소 보관 → 02 콜백 전달 → 03 콜백 등록·교체 → 04 객체와 사건 정보.
    같은 보상 함수를 유지하며 호출 책임을 조금씩 옮긴다.
    Callback은 call back, 요청받은 쪽이 지정된 함수를 되불러 주는 방식이다.
    함수 포인터는 주소를 담는 도구, 콜백은 그 도구를 쓰는 호출 방식이다.
    콜백은 비동기라는 뜻이 아니다. 이 예제는 모두 같은 흐름에서 즉시 호출한다.

 2. 함수 포인터 읽기
    void (*reward)(int) : int 하나를 받고 반환값이 없는 함수의 주소이다.
    void *reward(int)와 다르다. 별표와 이름을 괄호로 묶어야 함수 포인터이다.
    reward = AddGold; 는 주소를 보관한다. AddGold(20); 은 지금 호출한다.
    reward(20); 은 보관된 함수를 호출한다. (*reward)(20)도 같은 호출이다.
    대입할 함수의 반환형과 매개변수형을 일치시킨다. 억지 형변환은 하지 않는다.

 3. 02에서 바뀌는 것
    매번 전달하던 주소를 Chest.onOpen에 보관한다. opened도 추가한다.
    등록(Register)은 나중에 사용할 주소를 기록하는 일이다.
       등록 → 기다림 → 상자 열기 → 상태 변경 → 등록된 콜백 호출
    sound 상자는 효과음, treasure 상자는 골드를 준다. OpenChest는 동일하다.
    sound.onOpen = NULL은 등록 해제, = AddGold는 교체이다.
    실험: sound의 PrintSound를 AddGold로 바꾸면 최종 GOLD=40이다.
    기본 실행은 GOLD=20, 효과음 한 번이다. 같은 treasure를 두 번 열어도 20이다.
    여러 기능을 동시에 호출하는 구독은 Lecture13에서 다룬다.

==============================================================================
 [실습 코드] 먼저 실행 → 예측 → 추가 부분 작성 → 다시 실행
==============================================================================
*/

#include "../LectureSupport/console.h" // 화면 도구이다.
typedef void (*RewardCallback)(int); // [추가] 함수 포인터형에 이름을 붙인다.

int gold = 0; // 보상으로 쌓인 골드이다.

void AddGold(int amount) // 모든 단계에서 유지할 보상 함수이다.
{ // 보상 시작.
    gold += amount; // 받은 수량을 누적한다.
    printf("  REWARD +%d | GOLD=%d\n", amount, gold); // 결과를 표시한다.
} // 보상 끝.

typedef struct // [추가] 상자의 상태와 나중에 호출할 함수를 묶는다.
{ // 상자 필드 시작.
    int opened; // 한 번만 보상하기 위한 상태이다.
    RewardCallback onOpen; // 열릴 때 호출할 함수의 주소이다.
} Chest; // 상자 자료형이다.

void OpenChest(Chest *chest) // [변경] 등록된 콜백을 상자에서 꺼낸다.
{ // 상자 처리 시작.
    if (chest->opened) return; // 이미 열린 상자는 다시 보상하지 않는다.
    chest->opened = 1; // 콜백보다 먼저 상태를 변경해 중복 처리를 막는다.
    puts("  [##] -> [__] CHEST OPEN"); // 열린 상태를 표시한다.
    if (chest->onOpen != NULL) chest->onOpen(20); // 등록된 함수를 호출한다.
} // 상자 처리 끝.

void PrintSound(int amount) // AddGold와 같은 함수 모양을 지킨다.
{ // 효과음 반응 시작.
    printf("  SFX: coin sparkle (reward=%d)\n", amount); // 실제 오디오 대신 출력한다.
} // 효과음 반응 끝.

int main(void) // 게임 쪽에서 원하는 반응을 등록한다.
{ // main 시작.
    Chest treasure = {0, AddGold}; // 골드를 주는 상자이다.
    Chest sound = {0, PrintSound}; // 효과음만 표시하는 별도 상자이다.
    panel("12-03 | REGISTER / REPLACE"); // 제목이다.
    OpenChest(&treasure); // GOLD=20이 된다.
    OpenChest(&treasure); // 같은 상자는 재보상하지 않는다.
    OpenChest(&sound); // 같은 함수로 다른 반응을 얻는다.
    sound.onOpen = NULL; // 함수 주소를 지워 등록을 해제한다.
    return 0; // 정상 종료이다.
} // main 끝.
