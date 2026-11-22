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


============================================================================
[이벤트 기반 아키텍처 (Event-Driven Architecture, EDA) 개념 설명]
============================================================================

+-------------------------------------------------------+
|                    Event Loop (while)                 |
|                                                       |
|   [입력 / 사건 발생]                                  |
|          |                                            |
|          v                                            |
|   +-------------+     emit      +---------------+     |
|   |Event Emitter| ------------> |      Event    |     |
|   | (구독/발행) |               |  (type+data)  |     |
|   +-------------+               +---------------+     |
|                                         |             |
|                                    호출(함수포인터)   |
|                                         v             |
|                                 +---------------+     |
|                                 | Event Handler |     |
|                                 |   (처리 함수) |     |
|                                 +---------------+     |
+-------------------------------------------------------+

1. 한 줄 요약
   프로그램이 "지금 당장 할 일이 있나?" 하고 계속 묻거나 순서대로만 움직이는
   것이 아니라, "무슨 일(이벤트)이 발생했을 때" 미리 등록해 둔 반응(핸들러)을
   실행하는 구조임.
   예) 게임에서 플레이어가 공격 버튼을 누름 -> '공격 입력' 이벤트 발생 ->
       등록된 공격 처리 함수가 호출됨.


2. 핵심 4대 요소

   [1] 이벤트 (Event)
       - "무슨 일이 일어났는가"를 나타내는 사건 또는 그 정보를 담은 데이터임.
       - C언어에서는 보통 이벤트 종류(열거형 enum)와 부가 정보(구조체 struct)로
         표현함.
         예) EVT_KEY_PRESS (키 눌림), EVT_TIMER (시간 만료),
             EVT_COLLISION (충돌 발생, 어떤 물체끼리 부딪혔는지 데이터 포함)

   [2] 이벤트 핸들러 (Event Handler)
       - 특정 이벤트가 발생했을 때 실제로 실행할 "처리 함수"를 의미함.
       - C언어에서는 "나중에 호출될 함수의 주소"를 넘겨줘야 하므로, 보통
         '함수 포인터(Function Pointer)' 형태로 등록(구독, Subscribe)함.
         예) void (*EventHandler)(Event evt);
             등록할 때: subscribe(EVT_KEY_PRESS, on_key_pressed);
             (여기서 on_key_pressed가 바로 함수 이름이자 함수 주소/핸들러임)

   [3] 이벤트 이미터 (Event Emitter)
       - 이벤트를 만들어서 알리는 주체(발행자/중계자)임.
       - 핵심 역할 두 가지:
         (a) 구독(Subscribe / Register): "이 이벤트가 오면 이 함수를
             불러줘"라고 핸들러(함수 포인터)를 목록에 저장함.
         (b) 발행/전달(Emit / Dispatch): 실제 사건이 생겼을 때 이벤트를
             큐(Queue)에 넣거나, 해당 이벤트에 등록된 핸들러 함수들을
             순서대로 호출함.
       - 주의할 점: Emitter는 "누가 어떻게 처리하는지" 구체적 로직을 몰라도
         됨. "이런 일이 생겼다"고 알리고 연결해 주는 역할에 집중함.

   [4] 이벤트 루프 (Event Loop)
       - 쉽게 말해 "이벤트를 기다리고 꺼내서 처리하는 반복문(while 루프)"임.
       - 단순 while(1) 루프와 다른 점:
         일반 반복문은 매 프레임/매 회전마다 모든 검사를 직접 수행하는 경우가
         많지만, 이벤트 루프는 '발생한 이벤트(또는 입력/신호 대기)'를 확인하고,
         이벤트가 있으면 꺼내서(dequeue) 이미터/디스패처를 통해 연결된
         핸들러를 호출한 뒤 다시 다음 이벤트를 기다리는 흐름을 반복함.
       - 핵심 구조:
         while (running) {
             1. 이벤트 발생 여부 확인 또는 대기 (입력 폴링, 큐 확인, OS 대기 등)
             2. 처리할 이벤트 꺼내기
             3. 해당 이벤트에 구독된 핸들러(함수 포인터) 호출
         }


3. 전체 동작 흐름 (초보자용 비유: 식당 주문 벨과 알림판)
   - Event Handler : "치킨 주문이 오면 튀긴다"는 요리사(실제 작업 함수)
   - Event Emitter : 주문을 받아 알림판/벨을 울리는 카운터·시스템
   - Event         : "치킨 1마리 주문"이라는 주문서(사건 + 데이터)
   - Event Loop  : 카운터에서 주문이 들어왔는지 계속 확인하며 차례대로
                   요리사에게 일을 넘기는 운영 흐름

   [코드 흐름 예시]
   1) 초기화 단계:
      emitter_subscribe(EVT_HIT, handle_player_hit);
      -> EVT_HIT 이벤트가 발생하면 handle_player_hit 함수를 부르겠다고 등록
   2) 실행 단계(이벤트 루프):
      - 적 투사체가 명중함 -> EVT_HIT 이벤트 생성 및 발행(emit)
      - 이벤트 루프가 EVT_HIT 이벤트를 꺼냄
      - 이미터가 EVT_HIT에 등록된 handle_player_hit 함수 포인터를 호출함
      - handle_player_hit 내부에서 체력 감소, 피격 효과음 재생 등 수행


4. 왜 이 구조를 쓰는가? (장점과 트레이드오프)
   - 장점(결합도 감소, Decoupling):
     이벤트를 발생시키는 쪽(예: 충돌 감지 코드)이 사운드, UI, 점수, 이펙트
     코드를 직접 #include 하거나 직접 호출하지 않아도 됨. 새 반응(핸들러)을
     추가하거나 빼기 쉬워짐.
   - 단점(흐름 추적 어려움, 디버깅 주의):
     함수를 코드에서 바로 호출하는 것이 아니라 "어딘가에 등록된 함수 포인터가
     나중에 호출"되므로, 실행 순서를 눈으로 따라가기 어려울 수 있음.
     또한 핸들러 안에서 또 다른 이벤트를 연속 발행하면 연쇄 호출(재진입/
     순환) 문제가 생길 수 있으므로, 큐(Queue) 기반 처리와 종료 조건 관리가
     중요함.


5. C언어 구현 시 초보자가 꼭 챙겨야 할 포인트
   - 함수 포인터 시그니처 일치:
     등록하려는 함수의 반환형과 매개변수 타입이 핸들러 포인터 타입과
     정확히 맞아야 함.
   - 콜백 문맥(Context / user_data) 전달:
     C언어에는 객체(this)가 없으므로, 핸들러가 어떤 대상(예: 특정 플레이어
     구조체)을 다뤄야 하는지 알 수 있도록 `void* user_data`를 함께 넘기는
     패턴이 실무에서 매우 자주 쓰임.
   - 수명(Lifetime)과 메모리 관리:
     이벤트 데이터나 user_data가 핸들러가 실행되기 전에 소멸(스택 변수 해제,
     free 등)되지 않도록 주의해야 하며, 필요 없어진 핸들러는 구독 해제
     (unsubscribe)할 수 있어야 함.
   - 루프 종료 조건과 CPU 점유:
     종료 플래그(`running = 0` 또는 `EVT_QUIT`)를 두어 루프를 빠져나올 수
     있게 만들고, 대기 구간에서 불필요하게 CPU 100%를 소모하지 않도록
     입출력 대기(블로킹/이벤트 대기)나 적절한 대기/프레임 제어를 고려해야 함.
============================================================================


==============================================================================
 [실습 코드] 먼저 실행 → 예측 → 추가 부분 작성 → 다시 실행
==============================================================================
*/

#include <stdio.h>

// 제목을 테두리 안에 정렬한다. printf의 %-56s는 왼쪽 정렬 형식이다.
void panel(const char *title)
{
    puts("\n+==========================================================+");
    printf("| %-56s |\n", title);
    puts("+==========================================================+");
}

// 상태를 수정하지 않고 활성 객체만 경로 위에 그린다.
void track(int x, int active)
{
    fputs("  START |", stdout);
    for (int cell = 0; cell < 12; ++cell)
        printf(" %c ", active && cell == x ? '@' : '.');
    puts("| GOAL");
}

typedef struct GameObject GameObject; // 함수 포인터형에서 사용할 이름을 미리 선언한다.
typedef void (*GoalCallback)(GameObject *, int); // 12강의 대상+수량 콜백이다.
struct GameObject // 11강의 구조체에 콜백만 추가한다.
{
    int x;
    int active;
    GoalCallback onGoal; // 도착 시 실행할 반응이다.
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

int score = 0; // 도착한 주자의 점수를 기록한다.

enum { LISTENER_CAPACITY = 2 }; // 동시에 등록할 반응의 최대 수이다.
GoalCallback listeners[LISTENER_CAPACITY] = {0};
int listenerCount = 0;

int Subscribe(GoalCallback callback) // 사건 수신 함수를 등록한다.
{
    if (callback == NULL || listenerCount == LISTENER_CAPACITY) return 0; // 잘못된 등록 거절이다.
    listeners[listenerCount++] = callback;
    return 1;
}

void EmitGoal(GameObject *object, int amount) // 한 사건을 모든 구독자에게 전달한다.
{
    for (int i = 0; i < listenerCount; ++i)
        listeners[i](object, amount); // 같은 사건 정보를 각 반응에 전달한다.
}

void GoalEventHandler(GameObject* object, int amount) // 도착 사건에 대한 게임의 반응이다.
{
    score += amount; // 사건 하나에 한 번만 점수를 더한다.
    printf("  EVENT: GOAL x=%d | SCORE=%d\n", object->x, score);
}

void PlaySoundEventHandler(GameObject *object, int amount) // 게임 쪽의 두 번째 반응이다.
{
    score += amount;
    printf("  SFX: goal bell | x=%d reward=%d | SCORE=%d\n", object->x, amount, score); // 효과음 대신 출력한다.
}

void Update(GameObject *object) // 변수 대신 객체의 주소를 받는다.
{
    if (!object->active) return; // 비활성 객체는 움직이지 않는다.
    ++object->x; // ->는 포인터가 가리키는 구조체의 필드에 접근한다.
    if (object->x >= 11) // 도착 사건을 발견한다.
    {
        Destroy(object); // 먼저 상태를 확정하여 중복 도착을 막는다.
        if (object->onGoal != NULL) object->onGoal(object, 10); // 등록된 반응을 부른다.
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
        pool[i].onGoal = onGoal; // 이번 생애의 반응을 등록한다.
        printf("  SPAWN slot=%d\n", i);
        return; // 한 번의 요청에 한 객체만 생성한다.
    }
    puts("  POOL FULL | Wait for a runner to reach the gate."); // 가득 차면 거절한다.
}

int main(void) // 02의 게임 루프를 배열 순회로 확장한다.
{
    if (!Subscribe(GoalEventHandler) || !Subscribe(PlaySoundEventHandler)) return 1; // 게임 반응을 연결한다.
    panel("13-02 | ONE EVENT / TWO LISTENERS");
    puts("  s: spawn | Enter: step | q: quit | 3 runners max");
    Spawn(EmitGoal); // 첫 주자를 출발시킨다.
    while (1)
    {
        //input
        int key = getch();

        //update
        if (key == 'q' || key == EOF) break; // 종료를 처리한다.
        if (key == 's') Spawn(EmitGoal); // 생성 요청만 처리하고 시간은 진행하지 않는다.
        if (key == 13) // Enter를 눌렀을 때만 한 틱 갱신한다.
            for (int i = 0; i < CAPACITY; ++i) Update(&pool[i]); // 모든 객체 갱신이다.
        // 01에서는 콜백이 Update 안에서 즉시 실행된다.

        //render
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
