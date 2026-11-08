/*
==============================================================================
 [강의 자료] Lecture11_03 | Object Pool | 주자 대기실
==============================================================================

1. 같은 게임을 한 겹씩 확장한다
    01: 입력 → 이동 → 화면
    02: GameObject + Start / Update / Destroy
    03: 객체 하나 → 고정 배열 Pool
    04: 도착 사실 → OnGoal 이벤트 → 점수
    이전 프로젝트를 복사한 뒤 [추가] 부분을 붙인다. 각 완성본은 독립 실행된다.
    표시용 panel, track과 입력용 command는 제공 코드(console.h)이다.

 2. 게임 루프의 세 박자
       Input(입력) ---> Update(상태 변경) ---> Render(그리기)
           ^                                      |
           +--------------------------------------+
    Loop는 고리, Frame은 한 장의 화면이다. 한 바퀴에 상태를 한 번 바꾼다.
    이 수업은 Enter로 한 틱씩 진행하는 턴 방식이다. Tick은 시계의 한 눈금이다.
    실시간 게임은 입력을 기다리지 않고 반복하며 경과 시간도 고려해야 한다.
    아래 코드는 실시간 프레임 속도나 delta time(경과 시간)을 구현하지 않는다.
    Enter=진행, q=종료. 입력 파일 끝(EOF)도 종료로 처리한다.

 3. 02 위에 추가할 것
    hero 하나 → pool[3]. Start, Update, Destroy는 그대로 둔다.
    Spawn과 배열 순회만 추가한다. Pool은 함께 쓸 자원을 모아 둔 저장소이다.
       [0 ACTIVE] [1 FREE] [2 FREE] --Spawn--> [0 ACTIVE] [1 ACTIVE] [2 FREE]
    이 예제는 malloc/free 없이 칸을 재사용한다. 메모리 수명과 활성 수명은 다르다.
    가득 차면 새 요청을 거절한다. 활성 객체를 덮어쓰지 않는다.
    s는 생성만 하고 Enter는 시간만 진행한다. 입력마다 무엇이 변하는지 구분한다.
    실험: s 세 번 → 세 번째 요청은 POOL FULL.
    Enter 11번 뒤 s → slot=0, 위치=0 재사용. 이전 위치가 남으면 안 된다.

==============================================================================
 [실습 코드] 먼저 실행 → 예측 → 추가 부분 작성 → 다시 실행
==============================================================================
*/

#include "../LectureSupport/console.h" // 화면과 입력 도구이다.

typedef struct // [추가] struct는 서로 관련된 값의 구조를 정의한다.
{ // GameObject의 필드 시작.
    int x; // 위치이다.
    int active; // 활성 여부: 1이면 게임에 참여한다.
} GameObject; // typedef로 자료형에 짧은 이름을 붙인다.

void Start(GameObject *object) // [추가] 한 번의 활성화가 시작될 때 호출한다.
{ // Start 시작.
    object->x = 0; // 재사용할 때도 출발점으로 초기화한다.
    object->active = 1; // 게임에 참여시킨다.
} // Start 끝.

void Destroy(GameObject *object) // [추가] 게임 참여를 끝낸다.
{ // Destroy 시작.
    object->active = 0; // 메모리를 해제하지 않고 비활성으로 표시한다.
} // Destroy 끝.

void Update(GameObject *object) // [변경] 변수 대신 객체의 주소를 받는다.
{ // Update 시작.
    if (!object->active) return; // 비활성 객체는 움직이지 않는다.
    ++object->x; // ->는 포인터가 가리키는 구조체의 필드에 접근한다.
    if (object->x >= 11) Destroy(object); // 도착한 객체를 비활성화한다.
} // Update 끝.

enum { CAPACITY = 3 }; // [추가] 미리 마련할 객체 수이다.
GameObject pool[CAPACITY] = {0}; // 전역 배열의 모든 필드는 0으로 시작한다.

void Spawn(void) // [추가] 빈자리를 찾아 재사용한다.
{ // Spawn 시작.
    for (int i = 0; i < CAPACITY; ++i) // 배열의 모든 칸을 순서대로 검사한다.
    { // 슬롯 검사 시작.
        if (pool[i].active) continue; // 사용 중인 칸은 건너뛴다.
        Start(&pool[i]); // 이전 위치도 초기화하며 활성화한다.
        printf("  SPAWN slot=%d\n", i); // 어떤 칸을 사용했는지 확인한다.
        return; // 한 번의 요청에 한 객체만 생성한다.
    } // 슬롯 검사 끝.
    puts("  POOL FULL | Wait for a runner to reach the gate."); // 가득 차면 거절한다.
} // Spawn 끝.

int main(void) // 02의 게임 루프를 배열 순회로 확장한다.
{ // main 시작.
    panel("11-03 | RUNNER POOL"); // 현재 실습 제목이다.
    puts("  s: spawn | Enter: step | q: quit | 3 runners max"); // 조작법이다.
    Spawn(); // 첫 주자를 출발시킨다.
    for (;;) // 게임 루프이다.
    { // 한 틱 시작.
        int key = command(); // Input이다.
        if (key == 'q' || key == EOF) break; // 종료를 처리한다.
        if (key == 's') Spawn(); // 생성 요청만 처리하고 시간은 진행하지 않는다.
        if (key == '\n') // Enter를 눌렀을 때만 한 틱 갱신한다.
            for (int i = 0; i < CAPACITY; ++i) Update(&pool[i]); // 모든 객체 갱신이다.
        // 03에서는 추가 후처리가 없다.
        for (int i = 0; i < CAPACITY; ++i) // Render: 슬롯별로 경로를 그린다.
        { // 슬롯 출력 시작.
            printf("  SLOT %d | %s\n", i, pool[i].active ? "ACTIVE" : "FREE"); // 상태이다.
            track(pool[i].x, pool[i].active); // 비활성이면 빈 경로이다.
        } // 슬롯 출력 끝.
    } // 게임 루프 끝.
    for (int i = 0; i < CAPACITY; ++i) Destroy(&pool[i]); // 종료 시 참여를 끝낸다.
    return 0; // 정상 종료이다.
} // main 끝.
