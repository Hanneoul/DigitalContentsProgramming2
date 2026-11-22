/*
==============================================================================
 [강의 자료] Lecture13_01 | 미니 엔진 | 이동 코드는 보상을 모른다
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

 3. 11_04에서 교체할 부분
    GameObject에 onGoal 추가 → Spawn에서 등록 → Update의 OnGoal 직접 호출 교체.
    콜백 모양은 12_04와 같다. 객체와 보상량을 넘긴다.
    gold 대신 게임 전체 score를 사용하지만 호출 규약은 같다.
    Start는 이전 콜백을 NULL로 지운다. 재사용할 때 새 반응을 등록해야 한다.
    실험: main의 두 Spawn(OnGoal)을 Spawn(NULL)로 바꾸어 본다.
    예상: 주자는 이동하고 사라지지만 점수 반응은 발생하지 않는다.
    기본 실행: Enter 11번 → SCORE=10. 그 뒤 Enter는 재득점하지 않는다.
    즉시 콜백에서는 Spawn/Update 재호출을 하지 않는다. 반응 처리만 담당한다.

==============================================================================
 [실습 코드] 먼저 실행 → 예측 → 추가 부분 작성 → 다시 실행
==============================================================================
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
    int key = getchar();
    int rest = key;
    while (rest != '\n' && rest != EOF) rest = getchar();
    return key;
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

void OnGoal(GameObject *object, int amount) // 도착 사건에 대한 게임의 반응이다.
{
    score += amount; // 사건 하나에 한 번만 점수를 더한다.
    printf("  EVENT: GOAL x=%d | SCORE=%d\n", object->x, score);
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
    panel("13-01 | MINI ENGINE / CALLBACK");
    puts("  s: spawn | Enter: step | q: quit | 3 runners max");
    Spawn(OnGoal); // 첫 주자를 출발시킨다.
    for (;;)
    {
        int key = command();
        if (key == 'q' || key == EOF) break; // 종료를 처리한다.
        if (key == 's') Spawn(OnGoal); // 생성 요청만 처리하고 시간은 진행하지 않는다.
        if (key == '\n') // Enter를 눌렀을 때만 한 틱 갱신한다.
            for (int i = 0; i < CAPACITY; ++i) Update(&pool[i]); // 모든 객체 갱신이다.
        // 01에서는 콜백이 Update 안에서 즉시 실행된다.
        for (int i = 0; i < CAPACITY; ++i) // Render: 슬롯별로 경로를 그린다.
        {
            printf("  SLOT %d | %s\n", i, pool[i].active ? "ACTIVE" : "FREE");
            track(pool[i].x, pool[i].active); // 비활성이면 빈 경로이다.
        }
    }
    for (int i = 0; i < CAPACITY; ++i) Destroy(&pool[i]); // 종료 시 참여를 끝낸다.
    return 0;
}
