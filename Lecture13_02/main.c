/*
==============================================================================
 [강의 자료] Lecture13_02 | 이벤트 구독 | 도착 종소리와 점수를 함께
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

 3. 01 위에 추가할 것
    Subscribe, listeners 배열, EmitGoal을 추가한다.
    Spawn의 콜백만 OnGoal에서 EmitGoal로 바꾼다. Update는 그대로이다.
       도착 → EmitGoal → [0] OnGoal: 점수
                       → [1] PlaySound: 효과음 문구
    Subscribe는 구독, Listener는 듣는 대상이다. 한 사건을 여러 반응이 받는다.
    배열 순서대로 즉시 호출한다. 실제 소리 재생 대신 SFX 문장을 출력한다.
    용량은 2이다. NULL이나 세 번째 등록은 0(실패)을 반환한다.
    실험: PlaySound 등록을 빼면 점수만 바뀐다. 이동 코드는 수정하지 않는다.
    예상: 세 객체 도착 시 SCORE=30, SFX 세 번.
    단순화를 위해 실행 전에만 구독한다. 콜백 안에서 등록·해제·생성하지 않는다.
    다음 단계는 갱신 중에 반응을 실행하지 않고 사건을 잠시 보관한다.

==============================================================================
 [실습 코드] 먼저 실행 → 예측 → 추가 부분 작성 → 다시 실행
==============================================================================
*/

#include "../LectureSupport/console.h" // 화면과 입력이다.

typedef struct GameObject GameObject; // 함수 포인터형에서 사용할 이름을 미리 선언한다.
typedef void (*GoalCallback)(GameObject *, int); // 12강의 대상+수량 콜백이다.
struct GameObject // 11강의 구조체에 콜백만 추가한다.
{ // 필드 시작.
    int x; // 위치이다.
    int active; // 활성 여부이다.
    GoalCallback onGoal; // [추가] 도착 시 실행할 반응이다.
}; // 구조체 정의 끝.

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

int score = 0; // [추가] 도착한 주자의 점수를 기록한다.

void OnGoal(GameObject *object, int amount) // [추가] 도착 사건에 대한 게임의 반응이다.
{ // 이벤트 처리 시작.
    score += amount; // 사건 하나에 한 번만 점수를 더한다.
    printf("  EVENT: GOAL x=%d | SCORE=%d\n", object->x, score); // 사건과 결과이다.
} // 이벤트 처리 끝.

enum { LISTENER_CAPACITY = 2 }; // [추가] 동시에 등록할 반응의 최대 수이다.
GoalCallback listeners[LISTENER_CAPACITY] = {0}; // 빈 콜백 칸이다.
int listenerCount = 0; // 현재 등록된 수이다.

int Subscribe(GoalCallback callback) // [추가] 사건 수신 함수를 등록한다.
{ // 등록 시작.
    if (callback == NULL || listenerCount == LISTENER_CAPACITY) return 0; // 잘못된 등록 거절이다.
    listeners[listenerCount++] = callback; // 끝에 넣고 개수를 늘린다.
    return 1; // 등록 성공이다.
} // 등록 끝.

void EmitGoal(GameObject *object, int amount) // [추가] 한 사건을 모든 구독자에게 전달한다.
{ // 전달 시작.
    for (int i = 0; i < listenerCount; ++i) // 등록 순서로 방문한다.
        listeners[i](object, amount); // 같은 사건 정보를 각 반응에 전달한다.
} // 전달 끝.

void PlaySound(GameObject *object, int amount) // 게임 쪽의 두 번째 반응이다.
{ // 효과음 반응 시작.
    printf("  SFX: goal bell | x=%d reward=%d\n", object->x, amount); // 효과음 대신 출력한다.
} // 효과음 반응 끝.

void Update(GameObject *object) // [변경] 변수 대신 객체의 주소를 받는다.
{ // Update 시작.
    if (!object->active) return; // 비활성 객체는 움직이지 않는다.
    ++object->x; // ->는 포인터가 가리키는 구조체의 필드에 접근한다.
    if (object->x >= 11) // [변경] 도착 사건을 발견한다.
    { // 도착 처리 시작.
        Destroy(object); // 먼저 상태를 확정하여 중복 도착을 막는다.
        if (object->onGoal != NULL) object->onGoal(object, 10); // [변경] 등록된 반응을 부른다.
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
        pool[i].onGoal = onGoal; // [추가] 이번 생애의 반응을 등록한다.
        printf("  SPAWN slot=%d\n", i); // 어떤 칸을 사용했는지 확인한다.
        return; // 한 번의 요청에 한 객체만 생성한다.
    } // 슬롯 검사 끝.
    puts("  POOL FULL | Wait for a runner to reach the gate."); // 가득 차면 거절한다.
} // Spawn 끝.

int main(void) // 02의 게임 루프를 배열 순회로 확장한다.
{ // main 시작.
    if (!Subscribe(OnGoal) || !Subscribe(PlaySound)) return 1; // [추가] 게임 반응을 연결한다.
    panel("13-02 | ONE EVENT / TWO LISTENERS"); // 현재 실습 제목이다.
    puts("  s: spawn | Enter: step | q: quit | 3 runners max"); // 조작법이다.
    Spawn(EmitGoal); // 첫 주자를 출발시킨다.
    for (;;) // 게임 루프이다.
    { // 한 틱 시작.
        int key = command(); // Input이다.
        if (key == 'q' || key == EOF) break; // 종료를 처리한다.
        if (key == 's') Spawn(EmitGoal); // 생성 요청만 처리하고 시간은 진행하지 않는다.
        if (key == '\n') // Enter를 눌렀을 때만 한 틱 갱신한다.
            for (int i = 0; i < CAPACITY; ++i) Update(&pool[i]); // 모든 객체 갱신이다.
        // 01에서는 콜백이 Update 안에서 즉시 실행된다.
        for (int i = 0; i < CAPACITY; ++i) // Render: 슬롯별로 경로를 그린다.
        { // 슬롯 출력 시작.
            printf("  SLOT %d | %s\n", i, pool[i].active ? "ACTIVE" : "FREE"); // 상태이다.
            track(pool[i].x, pool[i].active); // 비활성이면 빈 경로이다.
        } // 슬롯 출력 끝.
    } // 게임 루프 끝.
    for (int i = 0; i < CAPACITY; ++i) Destroy(&pool[i]); // 종료 시 참여를 끝낸다.
    return 0; // 정상 종료이다.
} // main 끝.
