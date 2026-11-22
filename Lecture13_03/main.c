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


============================================================================
 [이벤트 큐(Event Queue) 개념 및 제공 소스코드 해설]
============================================================================

1. 이벤트 큐(Event Queue)란 무엇인가?
   - 사건(Event)이 발생한 즉시 핸들러(콜백)를 바로 실행하지 않고, 일단
     대기열(배열/큐)에 차곡차곡 쌓아 두었다가 정해진 시점에 순서대로
     꺼내 처리하는 구조임.
   - 보통 먼저 발생한 사건을 먼저 처리하는 FIFO(First-In, First-Out,
     선입선출) 방식으로 동작함.

2. 왜 이벤트 큐를 쓰는가? (즉시 호출 vs 큐 적재 후 일괄 디스패치)
   - 실행 순서 안정화:
     한 틱(프레임)에서 여러 객체(slot 0, 1, 2)를 갱신하는 도중 사건이
     생겨도, 갱신이 끝날 때까지 기다렸다가 한 번에 처리할 수 있음.
     (UPDATE -> EVENTS -> RENDER 단계 분리)
   - 재진입 / 상태 꼬임 방지:
     객체 이동·상태 변경 중간에 외부 반응(점수 갱신, 사운드, 객체 상태
     변경 등)이 끼어들어 루프 중 배열/객체 상태가 예상치 못하게 바뀌는
     상황을 줄임.
   - 용량 제한과 정책 결정:
     고정 크기 배열(QUEUE_CAPACITY)을 쓰면 큐가 가득 찼을 때 새 사건을
     버릴지(drop), 덮어쓸지, 큐를 확장할지 같은 정책을 명시적으로 다룰 수
     있음.

3. 이 소스코드의 전체 구조와 동작 흐름

   [1] 사건 및 객체 정의
       - GoalEvent: 도착 사건 정보(slot, x, amount)를 담는 구조체임.
       - GameObject: 위치(x), 활성 여부(active), 풀 인덱스(slot), 도착 시
         호출할 콜백(onGoal)을 가짐.
       - pool[CAPACITY]: 미리 할당해 둔 고정 크기 객체 풀임. Spawn()은
         비활성 슬롯을 찾아 Start()로 초기화해 재사용하고, Destroy()는
         active = 0으로만 바꿈.

   [2] 구독자(Listener / Event Handler) 등록
       - listeners[LISTENER_CAPACITY]: GoalEvent를 받을 함수 포인터 배열임.
       - Subscribe(): OnGoal, PlaySound 같은 콜백 함수를 등록함. 같은
         사건에 여러 구독자를 붙여 동시에 반응하게 만들 수 있음.

   [3] 이벤트 큐 적재(PostGoal)와 전달(Dispatch)
       - PostGoal(): 사건을 당장 실행하지 않고 events[eventCount++]에
         구조체 값(*event)을 복사해 넣음. 가득 차면 "EVENT QUEUE FULL"을
         출력하고 새 사건을 버림.
       - Dispatch(): 큐에 쌓인 사건(0 ~ eventCount - 1)을 순서대로 꺼내
         모든 구독자(listeners[0 .. listenerCount - 1])에게 &events[e]를
         넘겨 호출함. 처리가 끝나면 eventCount = 0으로 초기화해 다음 틱에
         배열을 재사용함.

   [4] 메인 루프 (UPDATE -> EVENTS -> RENDER)
       - 's': Spawn(PostGoal)으로 빈 슬롯에 주자를 배치하고, 이 주자의
         onGoal 콜백으로 PostGoal 함수 포인터를 연결함.
       - '\n'(Enter): 활성화된 모든 GameObject의 Update()를 수행함.
         x를 1 증가시키고 x >= 11이 되면 Destroy()로 비활성화한 뒤,
         지역 변수 GoalEvent를 만들어 onGoal(&event)를 호출함. 이때
         onGoal이 PostGoal이므로 지역 변수 주소가 사라지기 전에 구조체
         값이 events 배열에 복사됨.
       - Dispatch(): 한 틱의 갱신이 끝난 뒤 쌓인 GoalEvent들을 구독자
         (OnGoal, PlaySound)에게 전달해 점수와 사운드 로그를 출력함.
       - Render: 각 슬롯의 ACTIVE/FREE 상태와 경로(track)를 화면에 그림.

4. C언어 관점에서 눈여겨볼 핵심 포인트
   - 지역 변수 포인터와 값 복사:
     Update() 안의 GoalEvent event는 함수가 끝나면 사라지는 지역 변수임.
     PostGoal()이 포인터만 저장했다면 Dispatch() 시점에 잘못된 메모리를
     읽게 되므로, events[eventCount++] = *event 로 "값 자체를 복사"해
     안전성을 확보함.
   - 콜백의 역할 분리:
     GameObject는 "도착했다"는 사실과 GoalCallback 타입만 알 뿐, 실제로
     큐에 넣는지(PostGoal) 아니면 다른 방식으로 처리하는지는 Spawn 시점에
     주입된 함수 포인터에 의해 결정됨.
============================================================================

*/

#include <stdio.h>

// 제목을 테두리 안에 정렬한다. printf의 %-56s는 왼쪽 정렬 형식이다.
static void panel(const char *title)
{
    puts("\n+==========================================================+");
    printf("| %-56s |\n", title);
    puts("+==========================================================+");
}

// 상태를 수정하지 않고 활성 객체만 경로 위에 그린다.
static void track(int x, int active)
{
    fputs("  START |", stdout);
    for (int cell = 0; cell < 12; ++cell)
        printf(" %c ", active && cell == x ? '@' : '.');
    puts("| GOAL");
}

// getchar는 EOF도 반환하므로 int로 받는다. 한 명령 뒤 남은 줄을 소비한다.
static int command(void)
{
    int key = getch();
    
    return key;
}


typedef struct // 발생 순간의 값을 복사하는 사건 자료형이다.
{
    int slot; // 어느 풀 칸에서 발생했는지 기록한다.
    int x;
    int amount; // 이번 사건의 보상량이다.
} GoalEvent; // 다른 사건 종류는 별도 구조체로 확장할 수 있다.
typedef struct GameObject GameObject; // 객체 이름의 전방 선언이다.
typedef void (*GoalCallback)(const GoalEvent *); // 읽기 전용 사건을 받는다.
struct GameObject // 기존 객체에 풀 칸 번호만 추가한다.
{
    int x;
    int active;
    int slot; // 발생한 객체의 풀 칸이다.
    GoalCallback onGoal; // 사건을 큐에 넣는 콜백이다.
};

void Start(GameObject *object) // 한 번의 활성화가 시작될 때 호출한다.
{
    object->x = 0; // 재사용할 때도 출발점으로 초기화한다.
    object->onGoal = NULL; // 이전 사용자의 콜백을 지운다.
    object->active = 1; // 게임에 참여시킨다.
}

void Destroy(GameObject *object) // 게임 참여를 끝낸다.
{
    object->active = 0; // 메모리를 해제하지 않고 비활성으로 표시한다.
}

int score = 0; // 게임 쪽 누적 점수이다.

void OnGoal(const GoalEvent *event) // 객체 대신 발생 당시 사건을 읽는다.
{
    score += event->amount; // 전달된 수량만큼 보상한다.
    printf("  EVENT: GOAL slot=%d x=%d | SCORE=%d\n", event->slot, event->x, score);
}

void PlaySound(const GoalEvent *event) // 같은 사건을 두 번째 구독자도 받는다.
{
    printf("  SFX: goal bell | slot=%d\n", event->slot); // 실제 오디오 대신 출력한다.
}

enum { LISTENER_CAPACITY = 2 }; // 동시에 등록할 반응의 최대 수이다.
GoalCallback listeners[LISTENER_CAPACITY] = {0};
int listenerCount = 0;

int Subscribe(GoalCallback callback) // 사건 수신 함수를 등록한다.
{
    if (callback == NULL || listenerCount == LISTENER_CAPACITY) 
        return 0; // 잘못된 등록 거절이다.
    listeners[listenerCount++] = callback;
    return 1;
}


enum { QUEUE_CAPACITY = 3 }; // 한 틱에 객체 3개가 각 한 번 도착하는 최대 수이다.
GoalEvent events[QUEUE_CAPACITY]; // 사건 값을 보관할 배열이다.
int eventCount = 0; // 아직 처리하지 않은 사건 수이다.

void PostGoal(const GoalEvent *event) // 지금 반응하지 않고 사건을 복사한다.
{
    if (eventCount == QUEUE_CAPACITY) // 일반화할 때도 경계를 넘지 않는다.
    {
        puts("  EVENT QUEUE FULL | event dropped"); // 포화 정책: 새 사건을 버리고 알린다.
        return; // 배열 밖에 기록하지 않는다.
    }
    events[eventCount++] = *event; // 포인터가 아니라 사건의 값을 복사한다.
}

void Dispatch(void) // 갱신이 모두 끝난 뒤 모인 사건을 전달한다.
{
    for (int e = 0; e < eventCount; ++e) // 먼저 발생한 사건부터 처리한다.
        for (int i = 0; i < listenerCount; ++i) // 각 사건을 모든 구독자에게 보낸다.
            listeners[i](&events[e]); // 이 호출 동안만 사건 주소가 유효하다.
    eventCount = 0; // 처리한 배열 칸을 다음 틱에 재사용한다.
}

void Update(GameObject *object) // 변수 대신 객체의 주소를 받는다.
{
    if (!object->active) return; // 비활성 객체는 움직이지 않는다.
    ++object->x; // ->는 포인터가 가리키는 구조체의 필드에 접근한다.
    if (object->x >= 11) // 도착 사건을 발견한다.
    {
        Destroy(object); // 먼저 상태를 확정하여 중복 도착을 막는다.
        GoalEvent event = {object->slot, object->x, 10}; // 발생 순간의 정보를 묶는다.
        if (object->onGoal != NULL) object->onGoal(&event); // 큐가 이 지역 변수의 값을 복사한다.
    }
}

enum { CAPACITY = 3 }; // 미리 마련할 객체 수이다.
GameObject pool[CAPACITY] = {0}; // 전역 배열의 모든 필드는 0으로 시작한다.

void Spawn(GoalCallback onGoal) // 빈자리를 찾아 재사용한다.
{
    for (int i = 0; i < CAPACITY; ++i)
    {
        if (pool[i].active) continue; // 사용 중인 칸은 건너뛴다.
        Start(&pool[i]); // 재사용 상태를 초기화한다.
        pool[i].slot = i; // 사건에 넣을 풀 칸 번호이다.
        pool[i].onGoal = onGoal; // 이번 생애의 반응을 등록한다.
        printf("  SPAWN slot=%d\n", i);
        return; // 한 번의 요청에 한 객체만 생성한다.
    }
    puts("  POOL FULL | Wait for a runner to reach the gate."); // 가득 차면 거절한다.
}

int main(void) // 02의 게임 루프를 배열 순회로 확장한다.
{
    if (!Subscribe(OnGoal) || !Subscribe(PlaySound)) 
        return 1; // 게임 반응을 연결한다.

    panel("13-03 | UPDATE -> EVENTS -> RENDER");
    puts("  s: spawn | Enter: step | q: quit | 3 runners max");
    Spawn(PostGoal); // 첫 주자를 출발시킨다.

    while (1)
    {
        int key = command();
        system("cls");

        if (key == 'q' || key == EOF) break; // 종료를 처리한다.
        if (key == 's') Spawn(PostGoal); // 생성 요청만 처리하고 시간은 진행하지 않는다.
        if (key == 13) // Enter를 눌렀을 때만 한 틱 갱신한다.
            for (int i = 0; i < CAPACITY; ++i) 
                Update(&pool[i]); // 모든 객체 갱신이다.
        Dispatch(); // 모든 갱신 후 사건을 처리하고 화면을 그린다.
       
        
        for (int i = 0; i < CAPACITY; ++i) // Render: 슬롯별로 경로를 그린다.
        {
            printf("  SLOT %d | %s\n", i, pool[i].active ? "ACTIVE" : "FREE");
            track(pool[i].x, pool[i].active); // 비활성이면 빈 경로이다.
        }
    }

    for (int i = 0; i < CAPACITY; ++i) 
        Destroy(&pool[i]); // 종료 시 참여를 끝낸다.

    return 0;
}
