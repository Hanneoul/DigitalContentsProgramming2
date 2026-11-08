/*
==============================================================================
 [강의 자료] Lecture12_04 | 콜백의 인자 | 누구에게 생긴 사건인가
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

 3. 03에서 확장할 것
    Chest → GameObject, opened → active, onOpen → onGoal로 역할을 연결한다.
    콜백의 int 하나를 (GameObject *, int)로 확장하고 gold를 객체 안으로 옮긴다.
    Context(문맥)는 사건을 이해할 주변 정보이다. 여기서는 대상 객체가 그 정보이다.
    struct 이름을 먼저 선언하면 함수 포인터형에서도 그 포인터를 사용할 수 있다.
    함수 주소뿐 아니라 전달하는 객체 주소도 유효해야 한다.
    이 예제는 main의 객체가 살아 있는 동안 즉시 콜백을 호출한다.
    예상: 첫 객체 GOLD=20, 둘째 GOLD=20, 재호출한 첫 객체는 그대로 20.
    Lecture13에서는 ReachGoal의 호출 시점을 11강 Update의 도착 검사로 옮긴다.

==============================================================================
 [실습 코드] 먼저 실행 → 예측 → 추가 부분 작성 → 다시 실행
==============================================================================
*/

#include "../LectureSupport/console.h" // 화면 도구이다.

typedef struct GameObject GameObject; // 아래 구조체 이름을 먼저 알려 준다.
typedef void (*GoalCallback)(GameObject *, int); // 객체 주소와 보상량을 받는 함수형이다.

struct GameObject // [변경] 상자 대신 11강에서 만들었던 게임 객체이다.
{ // 필드 시작.
    int x; // 11강의 위치를 유지한다.
    int active; // 11강의 활성 상태를 유지한다.
    int gold; // [추가] 이 객체가 받은 보상이다.
    GoalCallback onGoal; // [추가] 도착 시 반응이다.
}; // 구조체 정의 끝.

void AddGold(GameObject *object, int amount) // [변경] 보상을 받을 대상도 받는다.
{ // 보상 시작.
    object->gold += amount; // 전역 골드 대신 해당 객체만 바꾼다.
    printf("  OBJECT GOLD=%d | REWARD=%d\n", object->gold, amount); // 결과이다.
} // 보상 끝.

void ReachGoal(GameObject *object) // 도착 사건을 간단히 재현하는 함수이다.
{ // 도착 시작.
    if (!object->active) return; // 이미 끝난 객체는 재보상하지 않는다.
    object->x = 11; // 도착 위치로 바꾼다.
    object->active = 0; // 콜백 전에 상태를 확정한다.
    if (object->onGoal != NULL) object->onGoal(object, 20); // 대상과 보상량을 전달한다.
} // 도착 끝.

int main(void) // 객체별 사건 정보를 확인한다.
{ // main 시작.
    GameObject first = {0, 1, 0, AddGold}; // 첫 객체의 보상은 독립적이다.
    GameObject second = {0, 1, 0, AddGold}; // 같은 콜백을 공유해도 상태는 다르다.
    panel("12-04 | WHO RECEIVES THE REWARD?"); // 제목이다.
    ReachGoal(&first); // 첫 객체가 20을 받는다.
    ReachGoal(&second); // 둘째 객체가 20을 받는다.
    ReachGoal(&first); // 비활성 객체는 중복 처리하지 않는다.
    return 0; // 정상 종료이다.
} // main 끝.
