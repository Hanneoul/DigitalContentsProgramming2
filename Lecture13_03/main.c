/*
==============================================================================
 [강의 자료] Lecture13_03 | 이벤트 큐 | 사건 접수와 처리를 나누기
==============================================================================

1. 두 강의가 만나는 곳
    Lecture11_04의 풀·생명주기·도착 검사 + Lecture12_04의 객체별 콜백.
       루프 → Update → 도착 사건 → 등록된 반응
    Engine(엔진)은 반복 실행과 객체 관리를 맡는 공통 장치이다.
    Event-driven은 사건(event)이 반응을 이끈다는 뜻이다.
    이벤트가 루프를 없애는 것은 아니다. 루프는 갱신하고 이벤트는 반응을 연결한다.
    라이브러리 없이 만든 작은 교육용 엔진이다. 실시간 시간 제어·충돌 엔진은 없다.

 2. 유지하는 조작
    s=생성만, Enter=한 틱 진행, q/EOF=종료. 시작 시 주자 하나가 있다.
    3칸이 모두 차면 생성 거절, 도착하면 비활성화, 다음 생성에서 빈칸 재사용.
    엔진 함수(Start/Destroy/Spawn/Update)와 게임 반응 함수를 구분해서 읽는다.

 3. 02에서 추가·교체할 부분
    GoalEvent, PostGoal, Dispatch를 추가하고 콜백 인자를 사건 포인터로 바꾼다.
    Spawn(EmitGoal) → Spawn(PostGoal), 모든 Update 다음에 Dispatch를 추가한다.
       Input → 모든 Update → [사건 배열] → Dispatch → Render
    Queue는 줄을 서서 기다리는 대기열이다. 이 예제는 틱마다 비우는 작은 배열 큐이다.
    먼저 발생한 사건부터 처리한다. 원형 큐나 여러 스레드 구현은 아니다.

 4. 주소 대신 사진을 보관하는 이유
    풀의 객체 주소만 저장하면 나중에 재사용된 객체의 상태를 읽을 수 있다.
    PostGoal은 *event를 복사한다. 지역 변수 event가 사라져도 기록은 남는다.
    slot은 발생 당시 칸 번호이며 영구적인 객체 ID가 아니다.
    수신자는 사건 주소를 보관하지 않는다. Dispatch 안에서만 읽는다.
    비활성화된 객체도 사건의 x와 amount는 보존된다.

 5. 작은 실험
    s 두 번 → Enter 11번. 세 사건이 slot 0,1,2 순서로 처리되고 SCORE=30이다.
    Enter를 더 눌러도 30, s 뒤 Enter 11번이면 SCORE=40이다.
    최대 세 객체가 틱당 한 번 발생하므로 큐 용량 3이면 충분하다.
    확장 시 포화 정책은 새 사건 폐기+안내이다. Subscribe도 용량을 검사한다.
    단순화 규칙: 콜백은 점수·표시만 변경한다. 처리 중 등록·해제·게시·생성 금지.
    다음 과제: 이 규칙을 풀려면 별도 다음 틱 큐와 객체 세대 번호가 왜 필요할까?

==============================================================================
 [실습 코드] 먼저 실행 → 예측 → 추가 부분 작성 → 다시 실행
==============================================================================
*/

#include "../LectureSupport/console.h" // 화면과 입력이다.

typedef struct // [추가] 발생 순간의 값을 복사하는 사건 자료형이다.
{ // 사건 필드 시작.
    int slot; // 어느 풀 칸에서 발생했는지 기록한다.
    int x; // 발생 당시 위치이다.
    int amount; // 이번 사건의 보상량이다.
} GoalEvent; // 다른 사건 종류는 별도 구조체로 확장할 수 있다.
typedef struct GameObject GameObject; // 객체 이름의 전방 선언이다.
typedef void (*GoalCallback)(const GoalEvent *); // [변경] 읽기 전용 사건을 받는다.
struct GameObject // 기존 객체에 풀 칸 번호만 추가한다.
{ // 객체 필드 시작.
    int x; // 위치이다.
    int active; // 활성 여부이다.
    int slot; // [추가] 발생한 객체의 풀 칸이다.
    GoalCallback onGoal; // 사건을 큐에 넣는 콜백이다.
}; // 객체 정의 끝.

void Start(GameObject *object) // [추가] 한 번의 활성화가 시작될 때 호출한다.
{ // Start 시작.
    object->x = 0; // 재사용할 때도 출발점으로 초기화한다.
    object->onGoal = NULL; // 이전 사용자의 콜백을 지운다.
    object->active = 1; // 게임에 참여시킨다.
} // Start 끝.

void Destroy(GameObject *object) // [추가] 게임 참여를 끝낸다.
{ // Destroy 시작.
    object->active = 0; // 메모리를 해제하지 않고 비활성으로 표시한다.
} // Destroy 끝.

int score = 0; // 게임 쪽 누적 점수이다.

void OnGoal(const GoalEvent *event) // [변경] 객체 대신 발생 당시 사건을 읽는다.
{ // 점수 처리 시작.
    score += event->amount; // 전달된 수량만큼 보상한다.
    printf("  EVENT: GOAL slot=%d x=%d | SCORE=%d\n", event->slot, event->x, score); // 결과이다.
} // 점수 처리 끝.

void PlaySound(const GoalEvent *event) // 같은 사건을 두 번째 구독자도 받는다.
{ // 효과음 처리 시작.
    printf("  SFX: goal bell | slot=%d\n", event->slot); // 실제 오디오 대신 출력한다.
} // 효과음 처리 끝.

enum { LISTENER_CAPACITY = 2 }; // [추가] 동시에 등록할 반응의 최대 수이다.
GoalCallback listeners[LISTENER_CAPACITY] = {0}; // 빈 콜백 칸이다.
int listenerCount = 0; // 현재 등록된 수이다.

int Subscribe(GoalCallback callback) // [추가] 사건 수신 함수를 등록한다.
{ // 등록 시작.
    if (callback == NULL || listenerCount == LISTENER_CAPACITY) return 0; // 잘못된 등록 거절이다.
    listeners[listenerCount++] = callback; // 끝에 넣고 개수를 늘린다.
    return 1; // 등록 성공이다.
} // 등록 끝.


enum { QUEUE_CAPACITY = 3 }; // 한 틱에 객체 3개가 각 한 번 도착하는 최대 수이다.
GoalEvent events[QUEUE_CAPACITY]; // [추가] 사건 값을 보관할 배열이다.
int eventCount = 0; // 아직 처리하지 않은 사건 수이다.

void PostGoal(const GoalEvent *event) // [추가] 지금 반응하지 않고 사건을 복사한다.
{ // 게시 시작.
    if (eventCount == QUEUE_CAPACITY) // 일반화할 때도 경계를 넘지 않는다.
    { // 포화 처리 시작.
        puts("  EVENT QUEUE FULL | event dropped"); // 포화 정책: 새 사건을 버리고 알린다.
        return; // 배열 밖에 기록하지 않는다.
    } // 포화 처리 끝.
    events[eventCount++] = *event; // 포인터가 아니라 사건의 값을 복사한다.
} // 게시 끝.

void Dispatch(void) // [추가] 갱신이 모두 끝난 뒤 모인 사건을 전달한다.
{ // 전달 시작.
    for (int e = 0; e < eventCount; ++e) // 먼저 발생한 사건부터 처리한다.
        for (int i = 0; i < listenerCount; ++i) // 각 사건을 모든 구독자에게 보낸다.
            listeners[i](&events[e]); // 이 호출 동안만 사건 주소가 유효하다.
    eventCount = 0; // 처리한 배열 칸을 다음 틱에 재사용한다.
} // 전달 끝.

void Update(GameObject *object) // [변경] 변수 대신 객체의 주소를 받는다.
{ // Update 시작.
    if (!object->active) return; // 비활성 객체는 움직이지 않는다.
    ++object->x; // ->는 포인터가 가리키는 구조체의 필드에 접근한다.
    if (object->x >= 11) // [변경] 도착 사건을 발견한다.
    { // 도착 처리 시작.
        Destroy(object); // 먼저 상태를 확정하여 중복 도착을 막는다.
        GoalEvent event = {object->slot, object->x, 10}; // [추가] 발생 순간의 정보를 묶는다.
        if (object->onGoal != NULL) object->onGoal(&event); // 큐가 이 지역 변수의 값을 복사한다.
    } // 도착 처리 끝.
} // Update 끝.

enum { CAPACITY = 3 }; // [추가] 미리 마련할 객체 수이다.
GameObject pool[CAPACITY] = {0}; // 전역 배열의 모든 필드는 0으로 시작한다.

void Spawn(GoalCallback onGoal) // [추가] 빈자리를 찾아 재사용한다.
{ // Spawn 시작.
    for (int i = 0; i < CAPACITY; ++i) // 배열의 모든 칸을 순서대로 검사한다.
    { // 슬롯 검사 시작.
        if (pool[i].active) continue; // 사용 중인 칸은 건너뛴다.
        Start(&pool[i]); // 재사용 상태를 초기화한다.
        pool[i].slot = i; // [추가] 사건에 넣을 풀 칸 번호이다.
        pool[i].onGoal = onGoal; // [추가] 이번 생애의 반응을 등록한다.
        printf("  SPAWN slot=%d\n", i); // 어떤 칸을 사용했는지 확인한다.
        return; // 한 번의 요청에 한 객체만 생성한다.
    } // 슬롯 검사 끝.
    puts("  POOL FULL | Wait for a runner to reach the gate."); // 가득 차면 거절한다.
} // Spawn 끝.

int main(void) // 02의 게임 루프를 배열 순회로 확장한다.
{ // main 시작.
    if (!Subscribe(OnGoal) || !Subscribe(PlaySound)) return 1; // [추가] 게임 반응을 연결한다.
    panel("13-03 | UPDATE -> EVENTS -> RENDER"); // 현재 실습 제목이다.
    puts("  s: spawn | Enter: step | q: quit | 3 runners max"); // 조작법이다.
    Spawn(PostGoal); // 첫 주자를 출발시킨다.
    for (;;) // 게임 루프이다.
    { // 한 틱 시작.
        int key = command(); // Input이다.
        if (key == 'q' || key == EOF) break; // 종료를 처리한다.
        if (key == 's') Spawn(PostGoal); // 생성 요청만 처리하고 시간은 진행하지 않는다.
        if (key == '\n') // Enter를 눌렀을 때만 한 틱 갱신한다.
            for (int i = 0; i < CAPACITY; ++i) Update(&pool[i]); // 모든 객체 갱신이다.
        Dispatch(); // [추가] 모든 갱신 후 사건을 처리하고 화면을 그린다.
        for (int i = 0; i < CAPACITY; ++i) // Render: 슬롯별로 경로를 그린다.
        { // 슬롯 출력 시작.
            printf("  SLOT %d | %s\n", i, pool[i].active ? "ACTIVE" : "FREE"); // 상태이다.
            track(pool[i].x, pool[i].active); // 비활성이면 빈 경로이다.
        } // 슬롯 출력 끝.
    } // 게임 루프 끝.
    for (int i = 0; i < CAPACITY; ++i) Destroy(&pool[i]); // 종료 시 참여를 끝낸다.
    return 0; // 정상 종료이다.
} // main 끝.
